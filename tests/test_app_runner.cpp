#include <gtest/gtest.h>
#include "app_runner.hpp"
#include <chrono>
#include <thread>
#include <csignal>
#include <future>
#include <filesystem>
#include <atomic>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class ThrowingWebSocketClient : public CWebSocketClient {
public:
    ThrowingWebSocketClient(net::io_context& ioc,
                           std::shared_ptr<CTradeQueue> tq,
                           const SAppConfig& cfg)
        : CWebSocketClient(ioc, tq, cfg) {}

    void Start() override {
        throw std::runtime_error("Simulated exception in WebSocketClient::Start()");
    }
};

class TestableAppRunner : public CAppRunner {
public:
    using CAppRunner::CAppRunner;
    using CAppRunner::StartWriter;
    using CAppRunner::StopWriter;
    void StartReader() override { }
    void StopReader() override { }
    using CAppRunner::m_cfg;
    using CAppRunner::m_trade_queue;
    using CAppRunner::m_aggregator;
    using CAppRunner::m_ioc;
    using CAppRunner::SetupSignalHandler;
    using CAppRunner::m_keep_running;
    using CAppRunner::m_signals;

    SAppConfig* config_ptr = nullptr;
    std::atomic<int> config_reload_count = 0;
    bool set_retry_zero = false;

    void set_config(SAppConfig* cfg) {
        config_ptr = cfg;
    }

    void set_ws_factory(std::function<std::shared_ptr<CWebSocketClient>(boost::asio::io_context&, std::shared_ptr<CTradeQueue>, const SAppConfig&)> factory) {
        m_ws_factory = factory;
    }

    void stop() {
        m_keep_running = false;
        m_ioc.stop();
        m_signals.reset();
    }

    bool LoadAndValidateConfig() override {
        config_reload_count++;
        if (config_ptr) {
            if (ValidateConfig(*config_ptr)) {
                m_cfg = *config_ptr;
                return true;
            }
            return false;
        }
        bool res = CAppRunner::LoadAndValidateConfig();
        if (res && set_retry_zero) m_cfg.retry.base_retry_sec = 0;
        return res;
    }
};

TEST(AppRunnerTest, Construct) {
    const char* argv[] = {"app", "--help"};
    CAppRunner runner(2, const_cast<char**>(argv));
    SUCCEED();
}

TEST(AppRunnerTest, LoadAndValidateConfig_Valid) {
    const char* argv[] = {"app"};
    TestableAppRunner runner(1, const_cast<char**>(argv));
    EXPECT_TRUE(runner.LoadAndValidateConfig());
}

TEST(AppRunnerTest, LoadAndValidateConfig_Invalid) {
    const char* argv[] = {"app", "--trade-pairs="};
    TestableAppRunner runner(2, const_cast<char**>(argv));
    EXPECT_FALSE(runner.LoadAndValidateConfig());
    int result = runner.Run();
    EXPECT_EQ(result, 1);
}

TEST(AppRunnerTest, StartStopWriterReader) {
    const char* argv[] = {"app"};
    TestableAppRunner runner(1, const_cast<char**>(argv));
    runner.m_trade_queue = std::make_shared<CTradeQueue>();
    runner.m_aggregator = std::make_shared<CTradeAggregator>(runner.m_cfg);
    runner.StartWriter();
    runner.StartReader();
    runner.StopWriter();
    runner.StopReader();
    SUCCEED();
}

TEST(AppRunnerTest, SignalInt) {
    const char* argv[] = {"app"};
    CAppRunner runner(1, const_cast<char**>(argv));

    std::future<int> result = std::async(std::launch::async, [&]() {
        return runner.Run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::raise(SIGINT);
    int exit_code = result.get();
    EXPECT_EQ(exit_code, 0);
}

TEST(AppRunnerTest, SignalPipeAndTerm) {
    const char* argv[] = {"app"};
    CAppRunner runner(1, const_cast<char**>(argv));

    std::future<int> result = std::async(std::launch::async, [&]() {
        return runner.Run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::raise(SIGPIPE);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Check that the program is still running (future is not ready)
    auto status = result.wait_for(std::chrono::milliseconds(0));
    EXPECT_EQ(status, std::future_status::timeout);
    std::raise(SIGTERM);
    int exit_code = result.get();
    EXPECT_EQ(exit_code, 0);
}

TEST(AppRunnerTest, SignalHangUpValidConfig) {
    const char* argv[] = {"app"};
    TestableAppRunner runner(1, const_cast<char**>(argv));

    SAppConfig initial_cfg;
    initial_cfg.agg.period_ms = 1000;
    initial_cfg.output.filename = "test1.log";
    runner.set_config(&initial_cfg);

    std::future<int> result = std::async(std::launch::async, [&]() {
        return runner.Run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(runner.m_cfg.agg.period_ms, 1000);
    EXPECT_EQ(runner.m_cfg.output.filename, "test1.log");

    initial_cfg.agg.period_ms = 2000;
    initial_cfg.output.filename = "test2.log";

    std::raise(SIGHUP);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto status = result.wait_for(std::chrono::milliseconds(0));
    EXPECT_EQ(status, std::future_status::timeout);

    EXPECT_EQ(runner.m_cfg.agg.period_ms, 2000);
    EXPECT_EQ(runner.m_cfg.output.filename, "test2.log");
    EXPECT_EQ(runner.config_reload_count.load(), 2);

    std::raise(SIGINT);
    int exit_code = result.get();
    EXPECT_EQ(exit_code, 0);
}

TEST(AppRunnerTest, SignalangUpInvalidConfig) {
    const char* argv[] = {"app"};
    TestableAppRunner runner(1, const_cast<char**>(argv));

    // Create initial valid config
    SAppConfig initial_cfg;
    initial_cfg.agg.period_ms = 1000;
    initial_cfg.output.filename = "test1.log";
    runner.set_config(&initial_cfg);

    std::future<int> result = std::async(std::launch::async, [&]() {
        return runner.Run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(runner.m_cfg.agg.period_ms, 1000);
    EXPECT_EQ(runner.m_cfg.output.filename, "test1.log");

    initial_cfg.agg.period_ms = 0;  // Invalid
    initial_cfg.output.filename = "test2.log";

    std::raise(SIGHUP);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto status = result.wait_for(std::chrono::milliseconds(0));
    EXPECT_EQ(status, std::future_status::timeout);

    EXPECT_EQ(runner.m_cfg.agg.period_ms, 1000);
    EXPECT_EQ(runner.m_cfg.output.filename, "test1.log");

    EXPECT_EQ(runner.config_reload_count.load(), 2);

    std::raise(SIGINT);
    int exit_code = result.get();
    EXPECT_EQ(exit_code, 0);
}

TEST(AppRunnerTest, ExceptionHandlingInLoop) {
    const char* argv[] = {"app"};
    TestableAppRunner runner(1, const_cast<char**>(argv));

    runner.set_retry_zero = true;
    ASSERT_TRUE(runner.LoadAndValidateConfig());
    runner.set_ws_factory([](boost::asio::io_context& ioc, std::shared_ptr<CTradeQueue> queue, const SAppConfig& cfg) {
        return std::make_shared<ThrowingWebSocketClient>(ioc, queue, cfg);
    });

    std::future<int> result = std::async(std::launch::async, [&]() {
        return runner.Run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    runner.stop();
    int exit_code = result.get();
    EXPECT_EQ(exit_code, 0);
}
