#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "websocket_client.hpp"
#include "trade_queue.hpp"
#include "config.hpp"
#include <memory>

class MockTradeQueue : public CTradeQueue {
public:
    MOCK_METHOD(void, Push, (STrade));
};

class TestWebSocketClient : public CWebSocketClient {
public:
    TestWebSocketClient(net::io_context& ioc, std::shared_ptr<CTradeQueue> tq, const SAppConfig& cfg)
        : CWebSocketClient(ioc, tq, cfg), start_connect_called(false) {}
    void StartConnect() override {
        start_connect_called = true;
        CWebSocketClient::StartConnect();
    }
    bool start_connect_called;
};

class WebSocketClientTestHelper {
public:
    static void call_schedule_reconnect(CWebSocketClient* client, std::string_view reason, boost::beast::error_code ec = {}) {
        client->ScheduleReconnect(reason, ec);
    }
    static void call_on_read(CWebSocketClient* client, boost::beast::error_code ec, std::size_t bytes) {
        client->OnRead(ec, bytes);
    }
    static void call_on_reconnect_timer(CWebSocketClient* client, boost::beast::error_code ec = {}) {
        client->OnReconnectTimer(ec);
    }
    static void prepare_buffer(CWebSocketClient* client, const std::string& data) {
        client->m_buffer.commit(boost::asio::buffer_copy(client->m_buffer.prepare(data.size()), boost::asio::buffer(data)));
    }
    static void init_ws(CWebSocketClient* client, net::io_context& ioc, ssl::context& ctx) {
        client->m_ws.emplace(ioc, ctx);
    }
};

TEST(WebSocketClientTest, ConstructStartStop) {
    boost::asio::io_context ioc;
    auto tq = std::make_shared<CTradeQueue>();
    SAppConfig cfg;
    auto client = std::make_shared<CWebSocketClient>(ioc, tq, cfg);
    client->Start();
    client->Stop();
    SUCCEED();
}

TEST(WebSocketClientTest, StopBeforeStart) {
    boost::asio::io_context ioc;
    auto tq = std::make_shared<CTradeQueue>();
    SAppConfig cfg;
    auto client = std::make_shared<CWebSocketClient>(ioc, tq, cfg);
    client->Stop();
    SUCCEED();
}

TEST(WebSocketClientTest, StartStopIdempotent) {
    boost::asio::io_context ioc;
    auto tq = std::make_shared<CTradeQueue>();
    SAppConfig cfg;
    auto client = std::make_shared<CWebSocketClient>(ioc, tq, cfg);
    client->Start();
    client->Start();
    client->Stop();
    client->Stop();
    SUCCEED();
}

TEST(WebSocketClientTest, InvalidConfigNoCrash) {
    boost::asio::io_context ioc;
    auto tq = std::make_shared<CTradeQueue>();
    SAppConfig cfg;
    cfg.ws.host = "";
    cfg.ws.port = "";
    auto client = std::make_shared<CWebSocketClient>(ioc, tq, cfg);
    client->Start();
    client->Stop();
    SUCCEED();
}

TEST(WebSocketClientTest, ReconnectionScheduledOnError) {
    boost::asio::io_context ioc;
    auto tq = std::make_shared<CTradeQueue>();
    SAppConfig cfg;
    auto client = std::make_shared<CWebSocketClient>(ioc, tq, cfg);
    EXPECT_FALSE(client->is_reconnect_scheduled());
    WebSocketClientTestHelper::call_schedule_reconnect(client.get(), "test_error");
    EXPECT_TRUE(client->is_reconnect_scheduled());
}

TEST(WebSocketClientTest, NoTradesOnReadError) {
    boost::asio::io_context ioc;
    auto mock_tq = std::make_shared<MockTradeQueue>();
    SAppConfig cfg;
    auto client = std::make_shared<CWebSocketClient>(ioc, mock_tq, cfg);
    EXPECT_CALL(*mock_tq, Push).Times(0);
    WebSocketClientTestHelper::call_on_read(client.get(), boost::beast::error_code{boost::beast::error::timeout}, 0);
}

TEST(WebSocketClientTest, ReconnectionCycle) {
    boost::asio::io_context ioc;
    auto tq = std::make_shared<CTradeQueue>();
    SAppConfig cfg;
    auto client = std::make_shared<TestWebSocketClient>(ioc, tq, cfg);
    EXPECT_FALSE(client->is_reconnect_scheduled());
    WebSocketClientTestHelper::call_schedule_reconnect(client.get(), "test_error");
    EXPECT_TRUE(client->is_reconnect_scheduled());
    EXPECT_FALSE(client->start_connect_called);
    WebSocketClientTestHelper::call_on_reconnect_timer(client.get(), {});
    EXPECT_FALSE(client->is_reconnect_scheduled());
    EXPECT_TRUE(client->start_connect_called);
}

TEST(WebSocketClientTest, TradesComeAfterSuccessfulRead) {
    boost::asio::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    auto mock_tq = std::make_shared<MockTradeQueue>();
    SAppConfig cfg;
    auto client = std::make_shared<CWebSocketClient>(ioc, mock_tq, cfg);
    WebSocketClientTestHelper::init_ws(client.get(), ioc, ctx);
    std::string json = R"({"s":"BTCUSDT","p":"100.0","q":"1.0","T":123456,"m":true})";
    WebSocketClientTestHelper::prepare_buffer(client.get(), json);
    EXPECT_CALL(*mock_tq, Push).Times(1);
    WebSocketClientTestHelper::call_on_read(client.get(), {}, json.size());
}

TEST(WebSocketClientTest, FullCycleInternetOnOffOn) {
    boost::asio::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    auto mock_tq = std::make_shared<MockTradeQueue>();
    SAppConfig cfg;
    auto client = std::make_shared<TestWebSocketClient>(ioc, mock_tq, cfg);
    WebSocketClientTestHelper::init_ws(client.get(), ioc, ctx);

    // 1. Internet on: successful read -> Push
    std::string json = R"({"s":"BTCUSDT","p":"100.0","q":"1.0","T":123456,"m":true})";
    WebSocketClientTestHelper::prepare_buffer(client.get(), json);
    EXPECT_CALL(*mock_tq, Push).Times(1);
    WebSocketClientTestHelper::call_on_read(client.get(), {}, json.size());

    // 2. Internet off: read error -> no Push, reconnection scheduled
    EXPECT_CALL(*mock_tq, Push).Times(0);
    WebSocketClientTestHelper::call_on_read(client.get(), boost::beast::error_code{boost::beast::error::timeout}, 0);
    EXPECT_TRUE(client->is_reconnect_scheduled());

    // 3. Recovery: timer expiry -> StartConnect called, reconnection reset
    WebSocketClientTestHelper::call_on_reconnect_timer(client.get(), {});
    EXPECT_FALSE(client->is_reconnect_scheduled());
    EXPECT_TRUE(client->start_connect_called);

    // 4. Internet on again: after reconnection successful read -> Push
    WebSocketClientTestHelper::prepare_buffer(client.get(), json);
    EXPECT_CALL(*mock_tq, Push).Times(1);
    WebSocketClientTestHelper::call_on_read(client.get(), {}, json.size());
}
