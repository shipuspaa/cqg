#include "websocket_client.hpp"

#include <algorithm>
#include <iostream>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "logger.hpp"

CWebSocketClient::CWebSocketClient(net::io_context& ioc, std::shared_ptr<CTradeQueue> tq, const SAppConfig& cfg)
        : m_ioc(ioc),
            m_ssl_ctx(ssl::context::tlsv12_client),
            m_resolver(ioc),
            m_reconnect_timer(ioc),
            m_trade_queue(tq),
            m_cfg(cfg) {
        m_ssl_ctx.set_default_verify_paths();
}

void CWebSocketClient::Start() {
    StartConnect();
}

void CWebSocketClient::Stop() {
    m_reconnect_timer.cancel();
    CloseConnection();
}

void CWebSocketClient::StartConnect() {
    m_reconnect_scheduled = false;
    m_ws.emplace(m_ioc, m_ssl_ctx);

    m_resolver.async_resolve(m_cfg.ws.host,
                             m_cfg.ws.port,
                             beast::bind_front_handler(&CWebSocketClient::OnResolve,
                                                       shared_from_this()));
}

void CWebSocketClient::OnResolve(beast::error_code ec,
                               tcp::resolver::results_type results) {
    if (!m_ws) {
        return;
    }
    if (ec) {
        ScheduleReconnect("resolve", ec);
        return;
    }

    beast::get_lowest_layer(*m_ws).async_connect(
        results,
        beast::bind_front_handler(&CWebSocketClient::OnConnect, shared_from_this()));
}

void CWebSocketClient::OnConnect(beast::error_code ec,
                               tcp::resolver::endpoint_type ep) {
    if (!m_ws) {
        return;
    }
    if (ec) {
        ScheduleReconnect("connect", ec);
        return;
    }

    if (!SSL_set_tlsext_host_name(m_ws->next_layer().native_handle(), m_cfg.ws.host.c_str())) {
        beast::error_code sni_ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        ScheduleReconnect("sni", sni_ec);
        return;
    }

    auto opt = websocket::stream_base::timeout::suggested(beast::role_type::client);
    opt.handshake_timeout = std::chrono::seconds(m_cfg.ws.handshake_timeout_sec);
    opt.idle_timeout = std::chrono::seconds(m_cfg.ws.idle_timeout_sec);
    m_ws->set_option(opt);

    m_ws->next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&CWebSocketClient::OnSslHandshake, shared_from_this()));
}

void CWebSocketClient::OnSslHandshake(beast::error_code ec) {
    if (!m_ws) {
        return;
    }
    if (ec) {
        ScheduleReconnect("ssl_handshake", ec);
        return;
    }

    std::string host_header = m_cfg.ws.host + ":" + m_cfg.ws.port;
    m_ws->async_handshake(
        host_header,
        m_cfg.trade_pairs.empty() ? "/" : BuildStreamTarget(m_cfg.trade_pairs),
        beast::bind_front_handler(&CWebSocketClient::OnWebsocketHandshake,
                                  shared_from_this()));
}

void CWebSocketClient::OnWebsocketHandshake(beast::error_code ec) {
    if (!m_ws) {
        return;
    }
    if (ec) {
        ScheduleReconnect("ws_handshake", ec);
        return;
    }

    m_retry_attempt = 0;

    Log(LogLevel::INFO, "Client", "Connected to Binance! Streaming trades...");
    m_ws->async_read(m_buffer,
                     beast::bind_front_handler(&CWebSocketClient::OnRead,
                                               shared_from_this()));
}

void CWebSocketClient::OnRead(beast::error_code ec,
                            std::size_t /*bytes_transferred*/) {
    if (!m_ws) {
        return;
    }
    if (ec) {
        ScheduleReconnect("read", ec);
        return;
    }

    try {
        STrade trade = STrade::FromJson(beast::buffers_to_string(m_buffer.data()));
        m_trade_queue->Push(std::move(trade));
    } catch (const std::exception& e) {
        Log(LogLevel::ERROR, "Client", "JSON Error: " + std::string(e.what()) + " | Data: " + beast::buffers_to_string(m_buffer.data()));
    }

    m_buffer.consume(m_buffer.size());
    m_ws->async_read(m_buffer,
                     beast::bind_front_handler(&CWebSocketClient::OnRead,
                                               shared_from_this()));
}

void CWebSocketClient::ScheduleReconnect(std::string_view reason, beast::error_code ec) {
    std::cout << "ScheduleReconnect" << std::endl;
    if (m_reconnect_scheduled) {
        return;
    }

    m_reconnect_scheduled = true;

    Log(LogLevel::ERROR, "Client", "Connection error (" + std::string(reason) + "): " + (ec ? ec.message() : "unknown"));

    CloseConnection();

    const auto reconnect_sec = std::min(
        std::chrono::seconds(m_cfg.retry.max_retry_sec),
        std::chrono::seconds(m_cfg.retry.base_retry_sec) * (1u << std::min(m_retry_attempt, 5u)));

    m_reconnect_timer.expires_after(reconnect_sec);
    m_reconnect_timer.async_wait(
        beast::bind_front_handler(&CWebSocketClient::OnReconnectTimer, shared_from_this()));
}

void CWebSocketClient::CloseConnection() {
    beast::error_code ec;
    m_resolver.cancel();
    if (m_ws && beast::get_lowest_layer(*m_ws).socket().is_open()) {
        m_ws->next_layer().shutdown(ec);
        beast::get_lowest_layer(*m_ws).close();
    }
    m_ws.reset();
    m_buffer.consume(m_buffer.size());
}

void CWebSocketClient::OnReconnectTimer(beast::error_code timer_ec) {
    if (timer_ec) {
        return;
    }
    if (m_retry_attempt < m_cfg.retry.max_retry_attempts) {
        ++m_retry_attempt;
    }
    StartConnect();
}