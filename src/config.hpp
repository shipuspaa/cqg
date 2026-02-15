#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SWebSocketConfig {
    std::string host = "stream.binance.com";
    std::string port = "9443";
    int handshake_timeout_sec = 10;
    int idle_timeout_sec = 10;
};

struct SRetryConfig {
    int base_retry_sec = 1;
    int max_retry_sec = 30;
    int max_retry_attempts = 32;
};

struct SAggregationConfig {
    uint64_t period_ms = 1000;

};

struct SOutputConfig {
    uint64_t write_period_ms = 5000;
    uint64_t write_delay_ms = 0;
    std::string filename = "aggregates.log";
    uint64_t max_file_mb = 10;
    uint64_t max_files = 10;
    bool console_report = false;
};

struct SAppConfig {
    std::vector<std::string> trade_pairs{"btcusdt", "ethusdt"};
    SWebSocketConfig ws;
    SRetryConfig retry;
    SAggregationConfig agg;
    SOutputConfig output;
};

SAppConfig LoadConfig(int argc, char** argv);
bool ValidateConfig(const SAppConfig& cfg);
std::string BuildStreamTarget(const std::vector<std::string>& pairs);
