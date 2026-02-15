#pragma once

#include "aggregator.hpp"
#include "websocket_client.hpp"
#include "config.hpp"
#include "trade_queue.hpp"
#include "logger.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <boost/asio.hpp>

class CAppRunner {
public:
    CAppRunner(int argc, char** argv);
    int Run();

    friend class CAppRunnerTestable;

protected:
    void SetupSignalHandler(std::shared_ptr<CWebSocketClient> ws_client);
    virtual bool LoadAndValidateConfig();
    void StartWriter();
    void StopWriter();
    virtual void StartReader();
    virtual void StopReader();
    void HandleExceptionBackoff();

    int m_argc = 0;
    char** m_argv = nullptr;

    SAppConfig m_cfg;

    boost::asio::io_context m_ioc;
    std::unique_ptr<net::signal_set> m_signals;

    std::shared_ptr<CTradeQueue> m_trade_queue;
    std::shared_ptr<CTradeAggregator> m_aggregator;

    std::atomic<bool> m_keep_running{true};
    std::atomic<bool> m_reload_requested{false};
    std::atomic<bool> m_writer_stop{false};

    uint32_t m_retry_attempt{0};

    std::function<std::shared_ptr<CWebSocketClient>(boost::asio::io_context&, std::shared_ptr<CTradeQueue>, const SAppConfig&)> m_ws_factory;

    std::thread m_writer;
    std::thread m_reader;
};
