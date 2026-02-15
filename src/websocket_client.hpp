#pragma once

#include "config.hpp"
#include "trade_queue.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
namespace ssl = net::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class CWebSocketClient : public std::enable_shared_from_this<CWebSocketClient> {
public:
    CWebSocketClient(net::io_context& ioc,
                   std::shared_ptr<CTradeQueue> tq,
                   const SAppConfig& cfg);
    virtual void Start();
    void Stop();

    bool is_reconnect_scheduled() const { return m_reconnect_scheduled; }

    friend class WebSocketClientTestHelper;

protected:
    virtual void StartConnect();

private:
    void OnResolve(beast::error_code ec, tcp::resolver::results_type results);
    void OnConnect(beast::error_code ec, tcp::resolver::endpoint_type endpoint);
    void OnSslHandshake(beast::error_code ec);
    void OnWebsocketHandshake(beast::error_code ec);
    void OnRead(beast::error_code ec, std::size_t bytes_transferred);
    void ScheduleReconnect(std::string_view reason, beast::error_code ec = {});
    void CloseConnection();
    void OnReconnectTimer(beast::error_code timer_ec);

    net::io_context& m_ioc;
    ssl::context m_ssl_ctx;
    tcp::resolver m_resolver;
    std::optional<websocket::stream<beast::ssl_stream<beast::tcp_stream>>> m_ws;
    beast::flat_buffer m_buffer;
    net::steady_timer m_reconnect_timer;

    SAppConfig m_cfg;

    std::shared_ptr<CTradeQueue> m_trade_queue;

    bool m_reconnect_scheduled{false};
    uint32_t m_retry_attempt{0};
};