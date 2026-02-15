
#include "aggregator.hpp"
#include <algorithm>
#include <chrono>

CTradeAggregator::CTradeAggregator(const SAppConfig& cfg)
    : m_cfg(cfg) {}

void CTradeAggregator::AddTrade(const STrade& trade) {
    if (!trade.IsValid()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    const uint64_t window_start = (trade.timestamp / m_cfg.agg.period_ms) * m_cfg.agg.period_ms;
    auto& stats = m_statistics[window_start][trade.symbol];
    stats.trades_count++;
    stats.total_quantity += trade.quantity;
    stats.total_volume += (trade.price * trade.quantity);
    stats.min_price = std::min(stats.min_price, trade.price);
    stats.max_price = std::max(stats.max_price, trade.price);
    if (trade.buyer_initiated) {
        stats.sell_count++;
    } else {
        stats.buy_count++;
    }

}

CTradeAggregator::AllWindowsStats CTradeAggregator::FlushStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    AllWindowsStats flushed;
    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    for (auto it = m_statistics.begin(); it != m_statistics.end();) {
        const uint64_t window_start = it->first;
        const uint64_t window_end = window_start + m_cfg.agg.period_ms;
        const uint64_t delay_ms = m_cfg.output.write_delay_ms;
        // We do not flush the window immediately after it ends,
        // we wait for delay_ms milliseconds. This allows for possible trade arrival delays,
        // so that late trades with timestamps falling into an already finished window are still aggregated correctly.
        // If agregate_using_timestamp=false, no delay is used and windows are flushed immediately
        if (window_end + delay_ms > now_ms) {
            ++it;
            continue;
        }
        flushed.emplace(window_start, std::move(it->second));
        it = m_statistics.erase(it);
    }
    
    return flushed;
}

void CTradeAggregator::UpdateConfig(const SAppConfig& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const bool statistics_affected =
        (cfg.agg.period_ms != m_cfg.agg.period_ms) ||
        (cfg.output.write_delay_ms != m_cfg.output.write_delay_ms);
    m_cfg = cfg;
    if (statistics_affected) {
        m_statistics.clear();
    }
}
