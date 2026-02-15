#include <gtest/gtest.h>
#include "trade.hpp"
#include <string>

TEST(TradeTest, IsValid) {
    STrade t{"BTCUSDT", 100.0, 1.0, 123456, true};
    EXPECT_TRUE(t.IsValid());
    STrade t1{"", 100.0, 1.0, 123456, true};
    EXPECT_FALSE(t1.IsValid());
    STrade t2{"BTCUSDT", -1.0, 1.0, 123456, true};
    EXPECT_FALSE(t2.IsValid());
    STrade t3{"BTCUSDT", 100.0, 0.0, 123456, true};
    EXPECT_FALSE(t3.IsValid());
    STrade t4{"BTCUSDT", 100.0, 1.0, 0, true};
    EXPECT_FALSE(t4.IsValid());
}


TEST(TradeTest, FromJson) {
    std::string json = R"({"s":"BTCUSDT","p":"100.0","q":"1.0","T":123456,"m":true})";
    STrade t = STrade::FromJson(json);
    EXPECT_EQ(t.symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(t.price, 100.0);
    EXPECT_DOUBLE_EQ(t.quantity, 1.0);
    EXPECT_EQ(t.timestamp, 123456u);
    EXPECT_TRUE(t.buyer_initiated);
    std::string bad_json = R"({"s":"","p":"100.0","q":"1.0","T":123456,"m":true})";
    EXPECT_THROW(STrade::FromJson(bad_json), std::runtime_error);
}