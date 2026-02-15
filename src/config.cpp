#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "logger.hpp"

namespace {
std::vector<std::string> SplitPairs(const std::string& raw) {
    std::vector<std::string> result;
    std::stringstream ss(raw);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item.erase(std::remove_if(item.begin(), item.end(), ::isspace), item.end());
        if (!item.empty()) {
            std::transform(item.begin(), item.end(), item.begin(), ::tolower);
            result.push_back(item);
        }
    }
    return result;
}

void ApplyJsonConfig(SAppConfig& cfg, const nlohmann::json& j) {
    if (j.contains("trade_pairs") && j["trade_pairs"].is_array()) {
        std::vector<std::string> pairs;
        for (const auto& p : j["trade_pairs"]) {
            if (p.is_string()) {
                auto s = p.get<std::string>();
                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                if (!s.empty()) {
                    pairs.push_back(s);
                }
            }
        }
        if (!pairs.empty()) {
            cfg.trade_pairs = std::move(pairs);
        }
    }

    // WebSocket config
    if (j.contains("ws") && j["ws"].is_object()) {
        auto& ws = j["ws"];
        if (ws.contains("host") && ws["host"].is_string()) cfg.ws.host = ws["host"];
        if (ws.contains("port") && ws["port"].is_string()) cfg.ws.port = ws["port"];
        if (ws.contains("handshake_timeout_sec") && ws["handshake_timeout_sec"].is_number_integer()) cfg.ws.handshake_timeout_sec = ws["handshake_timeout_sec"];
        if (ws.contains("idle_timeout_sec") && ws["idle_timeout_sec"].is_number_integer()) cfg.ws.idle_timeout_sec = ws["idle_timeout_sec"];
    }

    // Retry config
    if (j.contains("retry") && j["retry"].is_object()) {
        auto& retry = j["retry"];
        if (retry.contains("base_retry_sec") && retry["base_retry_sec"].is_number_integer()) cfg.retry.base_retry_sec = retry["base_retry_sec"];
        if (retry.contains("max_retry_sec") && retry["max_retry_sec"].is_number_integer()) cfg.retry.max_retry_sec = retry["max_retry_sec"];
        if (retry.contains("max_retry_attempts") && retry["max_retry_attempts"].is_number_integer()) cfg.retry.max_retry_attempts = retry["max_retry_attempts"];
    }

    // Aggregation config
    if (j.contains("agg") && j["agg"].is_object()) {
        auto& agg = j["agg"];
        if (agg.contains("period_ms") && agg["period_ms"].is_number_unsigned()) cfg.agg.period_ms = agg["period_ms"];

    }

    // Output config
    if (j.contains("output") && j["output"].is_object()) {
        auto& output = j["output"];
        if (output.contains("write_period_ms") && output["write_period_ms"].is_number_unsigned()) cfg.output.write_period_ms = output["write_period_ms"];
        if (output.contains("write_delay_ms") && output["write_delay_ms"].is_number_unsigned()) cfg.output.write_delay_ms = output["write_delay_ms"];
        if (output.contains("filename") && output["filename"].is_string()) cfg.output.filename = output["filename"];
        if (output.contains("max_file_mb") && output["max_file_mb"].is_number_unsigned()) cfg.output.max_file_mb = output["max_file_mb"];
        if (output.contains("max_files") && output["max_files"].is_number_unsigned()) cfg.output.max_files = output["max_files"];
        if (output.contains("console_report") && output["console_report"].is_boolean()) cfg.output.console_report = output["console_report"];
    }

    // Legacy fields for backward compatibility
    if (j.contains("agregate_period_ms") && j["agregate_period_ms"].is_number_unsigned()) {
        cfg.agg.period_ms = j["agregate_period_ms"].get<uint64_t>();
    }
    if (j.contains("write_period_ms") && j["write_period_ms"].is_number_unsigned()) {
        cfg.output.write_period_ms = j["write_period_ms"].get<uint64_t>();
    }
    if (j.contains("agregate_using_timestamp") && j["agregate_using_timestamp"].is_boolean()) {

    }
    if (j.contains("write_delay_ms") && j["write_delay_ms"].is_number_unsigned()) {
        cfg.output.write_delay_ms = j["write_delay_ms"].get<uint64_t>();
    }
    if (j.contains("output_filename") && j["output_filename"].is_string()) {
        cfg.output.filename = j["output_filename"].get<std::string>();
    }
    if (j.contains("max_file_mb") && j["max_file_mb"].is_number_unsigned()) {
        cfg.output.max_file_mb = j["max_file_mb"].get<uint64_t>();
    }
    if (j.contains("max_files") && j["max_files"].is_number_unsigned()) {
        cfg.output.max_files = j["max_files"].get<uint64_t>();
    }
    if (j.contains("console_report") && j["console_report"].is_boolean()) {
        cfg.output.console_report = j["console_report"].get<bool>();
    }
    if (j.contains("base_retry_sec") && j["base_retry_sec"].is_number_unsigned()) {
        cfg.retry.base_retry_sec = j["base_retry_sec"].get<int>();
    }
    if (j.contains("max_retry_sec") && j["max_retry_sec"].is_number_unsigned()) {
        cfg.retry.max_retry_sec = j["max_retry_sec"].get<int>();
    }
    if (j.contains("max_retry_attempts") && j["max_retry_attempts"].is_number_unsigned()) {
        cfg.retry.max_retry_attempts = j["max_retry_attempts"].get<int>();
    }
    if (j.contains("ws_host") && j["ws_host"].is_string()) {
        cfg.ws.host = j["ws_host"].get<std::string>();
    }
    if (j.contains("ws_port") && j["ws_port"].is_string()) {
        cfg.ws.port = j["ws_port"].get<std::string>();
    }
}

void ApplyCliOverrides(SAppConfig& cfg, int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--config=", 0) == 0) {
            continue;
        }
        if (arg.rfind("--trade-pairs=", 0) == 0) {
            cfg.trade_pairs = SplitPairs(arg.substr(14));
        } else if (arg.rfind("--agg-period-ms=", 0) == 0) {
            cfg.agg.period_ms = std::stoull(arg.substr(16));
        } else if (arg.rfind("--output-write-period-ms=", 0) == 0) {
            cfg.output.write_period_ms = std::stoull(arg.substr(24));
        } else if (arg.rfind("--agg-use-timestamp=", 0) == 0) {
            auto val = arg.substr(20);

        } else if (arg.rfind("--output-write-delay-ms=", 0) == 0) {
            cfg.output.write_delay_ms = std::stoull(arg.substr(23));
        } else if (arg.rfind("--output-filename=", 0) == 0) {
            cfg.output.filename = arg.substr(18);
        } else if (arg.rfind("--output-max-file-mb=", 0) == 0) {
            cfg.output.max_file_mb = std::stoull(arg.substr(20));
        } else if (arg.rfind("--output-max-files=", 0) == 0) {
            cfg.output.max_files = std::stoull(arg.substr(18));
        } else if (arg.rfind("--output-console-report=", 0) == 0) {
            auto val = arg.substr(23);
            cfg.output.console_report = (val == "1" || val == "true" || val == "TRUE");
        } else if (arg.rfind("--retry-base-retry-sec=", 0) == 0) {
            cfg.retry.base_retry_sec = std::stoull(arg.substr(23));
        } else if (arg.rfind("--retry-max-retry-sec=", 0) == 0) {
            cfg.retry.max_retry_sec = std::stoull(arg.substr(22));
        } else if (arg.rfind("--retry-max-retry-attempts=", 0) == 0) {
            cfg.retry.max_retry_attempts = static_cast<int>(std::stoull(arg.substr(27)));
        } else if (arg.rfind("--ws-host=", 0) == 0) {
            cfg.ws.host = arg.substr(10);
        } else if (arg.rfind("--ws-port=", 0) == 0) {
            cfg.ws.port = arg.substr(10);
        } else if (arg.rfind("--ws-handshake-timeout-sec=", 0) == 0) {
            cfg.ws.handshake_timeout_sec = std::stoi(arg.substr(27));
        } else if (arg.rfind("--ws-idle-timeout-sec=", 0) == 0) {
            cfg.ws.idle_timeout_sec = std::stoi(arg.substr(22));
        }
    }
}
}

SAppConfig LoadConfig(int argc, char** argv) {
    std::string config_path = "config.json";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--config=", 0) == 0) {
            config_path = arg.substr(9);
            break;
        }
    }
    SAppConfig cfg;
    std::ifstream in(config_path);
    if (in) {
        try {
            nlohmann::json j;
            in >> j;
            ApplyJsonConfig(cfg, j);
        } catch (const std::exception& e) {
            Log(LogLevel::ERROR, "Config", "Failed to parse config file: " + std::string(e.what()));
        }
    }
    try {
        ApplyCliOverrides(cfg, argc, argv);
    } catch (const std::exception& e) {
        Log(LogLevel::ERROR, "Config", "Invalid CLI overrides: " + std::string(e.what()));
        throw;
    }
    return cfg;
}

bool ValidateConfig(const SAppConfig& cfg) {
    if (cfg.trade_pairs.empty()) {
        Log(LogLevel::ERROR, "Config", "trade_pairs list is empty.");
        return false;
    }
    for (const auto& p : cfg.trade_pairs) {
        if (p.empty()) {
            Log(LogLevel::ERROR, "Config", "trade_pairs list contains empty symbol.");
            return false;
        }
        if (!std::all_of(p.begin(), p.end(), [](unsigned char c) { return std::isalnum(c); })) {
            Log(LogLevel::ERROR, "Config", "invalid symbol: " + p);
            return false;
        }
    }
    if (cfg.agg.period_ms == 0) {
        Log(LogLevel::ERROR, "Config", "agg.period_ms must be > 0.");
        return false;
    }
    if (cfg.output.write_period_ms == 0) {
        Log(LogLevel::ERROR, "Config", "output.write_period_ms must be > 0.");
        return false;
    }
    if (cfg.output.filename.empty()) {
        Log(LogLevel::ERROR, "Config", "output.filename is empty.");
        return false;
    }
    if (cfg.output.max_file_mb == 0) {
        Log(LogLevel::ERROR, "Config", "output.max_file_mb must be > 0.");
        return false;
    }
    if (cfg.output.max_files == 0) {
        Log(LogLevel::ERROR, "Config", "output.max_files must be > 0.");
        return false;
    }
    if (cfg.retry.base_retry_sec <= 0) {
        Log(LogLevel::ERROR, "Config", "retry.base_retry_sec must be > 0.");
        return false;
    }
    if (cfg.retry.max_retry_sec <= 0) {
        Log(LogLevel::ERROR, "Config", "retry.max_retry_sec must be > 0.");
        return false;
    }
    if (cfg.retry.max_retry_attempts <= 0) {
        Log(LogLevel::ERROR, "Config", "retry.max_retry_attempts must be > 0.");
        return false;
    }
    if (cfg.ws.host.empty()) {
        Log(LogLevel::ERROR, "Config", "ws.host must not be empty.");
        return false;
    }
    if (cfg.ws.port.empty()) {
        Log(LogLevel::ERROR, "Config", "ws.port must not be empty.");
        return false;
    }
    if (cfg.ws.handshake_timeout_sec <= 0) {
        Log(LogLevel::ERROR, "Config", "ws.handshake_timeout_sec must be > 0.");
        return false;
    }
    if (cfg.ws.idle_timeout_sec < 0) {  // 0 allowed to disable
        Log(LogLevel::ERROR, "Config", "ws.idle_timeout_sec must be >= 0.");
        return false;
    }
    {
        std::ofstream probe(cfg.output.filename, std::ios::app);
        if (!probe) {
            Log(LogLevel::ERROR, "Config", "output.filename is not writable: " + cfg.output.filename);
            return false;
        }
    }
    return true;
}

std::string BuildStreamTarget(const std::vector<std::string>& pairs) {
    std::ostringstream oss;
    oss << "/stream?streams=";
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (i > 0) {
            oss << "/";
        }
        oss << pairs[i] << "@trade";
    }
    return oss.str();
}
