#include "app_runner.hpp"
#include "logger.hpp"

#include <algorithm>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;

CAppRunner::CAppRunner(int argc, char** argv)
    : m_argc(argc), m_argv(argv),
      m_ws_factory([](boost::asio::io_context& ioc, std::shared_ptr<CTradeQueue> queue, const SAppConfig& cfg) {
          return std::make_shared<CWebSocketClient>(ioc, queue, cfg);
      }) {}

int CAppRunner::Run() {    
    if (!LoadAndValidateConfig()) {
        return 1;
    }

    //We handle such case so there is no need to terminate
    std::signal(SIGPIPE, SIG_IGN);
    m_signals = std::make_unique<net::signal_set>(m_ioc, SIGINT, SIGTERM, SIGHUP);

    m_trade_queue = std::make_shared<CTradeQueue>();
    m_aggregator = std::make_shared<CTradeAggregator>(m_cfg);

    StartWriter();
    StartReader();

    while (m_keep_running.load()) {
        try {
            if (m_reload_requested.exchange(false)) {
                Log(LogLevel::INFO, "Main", "Reloading config...");
                if (LoadAndValidateConfig()) {
                    m_aggregator->UpdateConfig(m_cfg);
                    StopWriter();
                    StartWriter();
                }
            }
            m_ioc.restart();
            auto ws_client = m_ws_factory(m_ioc, m_trade_queue, m_cfg);
            SetupSignalHandler(ws_client);
            ws_client->Start();
            Log(LogLevel::INFO, "Main", "Entering ioc.run()...");
            m_ioc.run();
            m_retry_attempt = 0;
        } catch (const std::exception& e) {
            Log(LogLevel::ERROR, "Main", "Exception in loop: " + std::string(e.what()));
            HandleExceptionBackoff();
        } catch (...) {
            Log(LogLevel::ERROR, "Main", "Unknown exception in loop.");
            HandleExceptionBackoff();
        }
    }

    StopWriter();
    StopReader();

    Log(LogLevel::INFO, "Main", "GQC service stopped safely.");
    return 0;
}

void CAppRunner::SetupSignalHandler(std::shared_ptr<CWebSocketClient> ws_client) {
    m_signals->clear();
    m_signals->add(SIGINT);
    m_signals->add(SIGTERM);
    m_signals->add(SIGHUP);

    m_signals->async_wait([this, ws_client](const beast::error_code& ec, int signo) {
        if (ec) return;

        if (signo == SIGHUP) {
            Log(LogLevel::INFO, "Main", "SIGHUP received.");
            m_reload_requested.store(true);
        } else {
            Log(LogLevel::INFO, "Main", "Shutdown signal received.");
            m_keep_running.store(false);
            m_trade_queue->Stop();
        }

        if (ws_client) ws_client->Stop();
        m_ioc.stop();
    });
}

bool CAppRunner::LoadAndValidateConfig() {
    try {
        m_cfg = LoadConfig(m_argc, m_argv);
    } catch (...) {
        Log(LogLevel::ERROR, "Config", "Failed to load config.");
        return false;
    }
    if (!ValidateConfig(m_cfg)) {
        Log(LogLevel::ERROR, "Config", "Validation failed.");
        return false;
    }
    return true;
}

void CAppRunner::StartWriter() {
    m_writer_stop.store(false);
    m_writer = std::thread([this]() {
        while (!m_writer_stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_cfg.output.write_period_ms));

            auto windows_stats = m_aggregator->FlushStatistics();
            if (windows_stats.empty()) {
                continue;
            }

            RotateLogsIfNeeded(m_cfg.output.filename, m_cfg.output.max_file_mb * 1024ull * 1024ull, m_cfg.output.max_files);

            std::ofstream out(m_cfg.output.filename, std::ios::app);
            if (!out) {
                Log(LogLevel::ERROR, "Writer", "Failed to open output file: " + m_cfg.output.filename);
                continue;
            }

            auto write_window = [](std::ostream& os, uint64_t window_start_ms, const CTradeAggregator::WindowStats& stats_map) {
                os << "timestamp=" << FormatIsoUtc(window_start_ms) << "\n";
                for (const auto& entry : stats_map) {
                    const auto& symbol = entry.first;
                    const auto& stats = entry.second;
                    os << "symbol=" << symbol
                       << " trades=" << stats.trades_count
                       << " volume=" << std::fixed << std::setprecision(5) << stats.total_volume
                       << " quantity=" << std::fixed << std::setprecision(5) << stats.total_quantity
                       << " min=" << std::setprecision(2) << stats.min_price
                       << " max=" << std::setprecision(2) << stats.max_price
                       << " buy=" << stats.buy_count
                       << " sell=" << stats.sell_count
                       << "\n";
                }
            };

            for (const auto& window_pair : windows_stats) {
                write_window(out, window_pair.first, window_pair.second);
                if (m_cfg.output.console_report) {
                    write_window(std::cout, window_pair.first, window_pair.second);
                }
            }
            out.flush();
            if (m_cfg.output.console_report) {
                std::cout.flush();
            }
        }
        Log(LogLevel::INFO, "Writer", "Thread finished.");
    });
}

void CAppRunner::StopWriter() {
    m_writer_stop.store(true);
    if (m_writer.joinable()) {
        m_writer.join();
    }
}

void CAppRunner::StartReader() {
    m_reader = std::thread([this]() {
        STrade t;
        while (m_trade_queue->Pop(t)) {
            m_aggregator->AddTrade(t);
        }
        Log(LogLevel::INFO, "Reader", "Thread finished.");
    });
}

void CAppRunner::StopReader() {
    if (m_reader.joinable()) {
        m_reader.join();
    }
}

void CAppRunner::HandleExceptionBackoff() {
    if (!m_keep_running.load()) {
        return;
    }
    //Need to stop ioc to be sure that we don't handle signals from OS while sleeping
    //this provides OS to terminate process
    m_ioc.stop();
    const std::chrono::seconds base_retry{m_cfg.retry.base_retry_sec};
    const std::chrono::seconds max_retry{m_cfg.retry.max_retry_sec};
    const auto sleep_sec = std::chrono::seconds(std::max(1, static_cast<int>(std::min(
        max_retry,
        base_retry * (1u << std::min(m_retry_attempt, 5u))).count())));
    Log(LogLevel::INFO, "Main", "Restart after exception in " + std::to_string(sleep_sec.count()) + "s...");
    std::this_thread::sleep_for(sleep_sec);
    if (!m_keep_running.load()) {
        return;
    }
    if (m_retry_attempt < m_cfg.retry.max_retry_attempts) {
        ++m_retry_attempt;
    }
}

