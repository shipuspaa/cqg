#include "logger.hpp"

#include "aggregator.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

std::string FormatIsoUtc(uint64_t timestamp_ms) {
    using namespace std::chrono;
    const auto tp = system_clock::time_point{milliseconds(timestamp_ms)};
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

void Log(LogLevel level, std::string_view component, std::string_view message) {
    auto now = std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&time_t, &tm);
    std::ostream& os = (level == LogLevel::ERROR) ? std::cerr : std::cout;
    os << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S]") << " [" << (level == LogLevel::INFO ? "INFO" : "ERROR") << "] [" << component << "] " << message << std::endl;
}

void RotateLogsIfNeeded(const std::string& path, uint64_t max_bytes, uint64_t max_files) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return;
    }
    const auto size = fs::file_size(path, ec);
    if (ec || size < max_bytes) {
        return;
    }

    const fs::path base(path);
    const fs::path dir = base.parent_path();
    const std::string filename = base.filename().string();

    // Remove the oldest
    fs::path oldest = dir / (filename + "." + std::to_string(max_files));
    if (fs::exists(oldest, ec)) {
        fs::remove(oldest, ec);
    }

    // Shift existing files
    for (uint64_t i = max_files; i > 1; --i) {
        fs::path from = dir / (filename + "." + std::to_string(i - 1));
        fs::path to = dir / (filename + "." + std::to_string(i));
        if (fs::exists(from, ec)) {
            fs::rename(from, to, ec);
        }
    }

    // Rotate current
    fs::path rotated = dir / (filename + ".1");
    fs::rename(base, rotated, ec);
}

