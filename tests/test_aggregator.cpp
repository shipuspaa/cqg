#include <gtest/gtest.h>
#include <thread>
#include "aggregator.hpp"
#include "trade.hpp"



TEST(AggregatorTest, Aggregate_UsingTimestamp) {
    SAppConfig cfg;

    CTradeAggregator agg(cfg);


    agg.AddTrade({"BTCUSDT", 100.0, 1.0, 1000, true});
    agg.AddTrade({"", 100.0, 1.0, 1000, true}); // not valid
    agg.AddTrade({"BTCUSDT", 110.0, 2.0, 1000, false});
    agg.AddTrade({"BTCUSDT", -1.0, 1.0, 1000, true}); // not valid
    agg.AddTrade({"BTCUSDT", 120.0, 1.0, 2000, false});
    agg.AddTrade({"BTCUSDT", 100.0, -1.0, 1000, true}); // not valid
    agg.AddTrade({"BTCUSDT", 100.0, 1.0, 0, true}); // not valid

    agg.AddTrade({"ETHUSDT", 200.0, 1.5, 1000, false});
    agg.AddTrade({"ETHUSDT", 210.0, 2.0, 2000, true});

    auto windows_stats = agg.FlushStatistics();
    ASSERT_EQ(windows_stats.size(), 2u);

    auto it = windows_stats.find(1000);
    ASSERT_NE(it, windows_stats.end());
    const auto& win1000 = it->second;
    ASSERT_EQ(win1000.size(), 2u);
    {
        const auto& stats_btc = win1000.at("BTCUSDT");
        EXPECT_EQ(stats_btc.trades_count, 2u);
        EXPECT_DOUBLE_EQ(stats_btc.total_quantity, 3.0);
        EXPECT_DOUBLE_EQ(stats_btc.total_volume, 320.0);
        EXPECT_DOUBLE_EQ(stats_btc.min_price, 100.0);
        EXPECT_DOUBLE_EQ(stats_btc.max_price, 110.0);
        EXPECT_EQ(stats_btc.buy_count, 1u);
        EXPECT_EQ(stats_btc.sell_count, 1u);
    }
    {
        const auto& stats_eth = win1000.at("ETHUSDT");
        EXPECT_EQ(stats_eth.trades_count, 1u);
        EXPECT_DOUBLE_EQ(stats_eth.total_quantity, 1.5);
        EXPECT_DOUBLE_EQ(stats_eth.total_volume, 300.0);
        EXPECT_DOUBLE_EQ(stats_eth.min_price, 200.0);
        EXPECT_DOUBLE_EQ(stats_eth.max_price, 200.0);
        EXPECT_EQ(stats_eth.buy_count, 1u);
        EXPECT_EQ(stats_eth.sell_count, 0u);
    }

    it = windows_stats.find(2000);
    ASSERT_NE(it, windows_stats.end());
    const auto& win2000 = it->second;
    ASSERT_EQ(win2000.size(), 2u);
    {
        const auto& stats_btc = win2000.at("BTCUSDT");
        EXPECT_EQ(stats_btc.trades_count, 1u);
        EXPECT_DOUBLE_EQ(stats_btc.total_quantity, 1.0);
        EXPECT_DOUBLE_EQ(stats_btc.total_volume, 120.0);
        EXPECT_DOUBLE_EQ(stats_btc.min_price, 120.0);
        EXPECT_DOUBLE_EQ(stats_btc.max_price, 120.0);
        EXPECT_EQ(stats_btc.buy_count, 1u);
        EXPECT_EQ(stats_btc.sell_count, 0u);
    }
    {
        const auto& stats_eth = win2000.at("ETHUSDT");
        EXPECT_EQ(stats_eth.trades_count, 1u);
        EXPECT_DOUBLE_EQ(stats_eth.total_quantity, 2.0);
        EXPECT_DOUBLE_EQ(stats_eth.total_volume, 420.0);
        EXPECT_DOUBLE_EQ(stats_eth.min_price, 210.0);
        EXPECT_DOUBLE_EQ(stats_eth.max_price, 210.0);
        EXPECT_EQ(stats_eth.buy_count, 0u);
        EXPECT_EQ(stats_eth.sell_count, 1u);
    }
}



