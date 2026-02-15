#pragma once
#include "config.hpp"
#include "trade.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>


class CTradeAggregator {
public:
    struct SSymbolStats {
        uint64_t trades_count = 0;
        double total_quantity = 0.0;
        double total_volume = 0.0;       // sum of price * quantity
        double min_price = std::numeric_limits<double>::max();
        double max_price = std::numeric_limits<double>::lowest();
        uint64_t buy_count = 0;
        uint64_t sell_count = 0;
    };

    using WindowStats = std::unordered_map<std::string, SSymbolStats>;
    using AllWindowsStats = std::map<uint64_t, WindowStats>;

    explicit CTradeAggregator(const SAppConfig& cfg);

    void AddTrade(const STrade& trade);
    AllWindowsStats FlushStatistics();
    void UpdateConfig(const SAppConfig& cfg);
private:

    SAppConfig m_cfg;
    AllWindowsStats m_statistics;
    std::mutex m_mutex;
};