#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

enum class LogLevel { INFO, ERROR };
void Log(LogLevel level, std::string_view component, std::string_view message);

std::string FormatIsoUtc(uint64_t timestamp_ms);
void RotateLogsIfNeeded(const std::string& path, uint64_t max_bytes, uint64_t max_files);
