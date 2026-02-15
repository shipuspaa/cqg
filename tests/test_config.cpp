#include <gtest/gtest.h>
#include "config.hpp"

TEST(ConfigTest, ValidateConfig_Valid) {
    SAppConfig cfg;
    EXPECT_TRUE(ValidateConfig(cfg));
}

TEST(ConfigTest, ValidateConfig_InvalidTradePairs) {
    SAppConfig cfg;
    cfg.trade_pairs.clear();
    EXPECT_FALSE(ValidateConfig(cfg));
}

TEST(ConfigTest, ValidateConfig_InvalidWsHost) {
    SAppConfig cfg;
    cfg.ws.host = "";
    EXPECT_FALSE(ValidateConfig(cfg));
}

TEST(ConfigTest, ValidateConfig_InvalidWsPort) {
    SAppConfig cfg;
    cfg.ws.port = "";
    EXPECT_FALSE(ValidateConfig(cfg));
}

TEST(ConfigTest, ValidateConfig_InvalidAggPeriod) {
    SAppConfig cfg;
    cfg.agg.period_ms = 0;
    EXPECT_FALSE(ValidateConfig(cfg));
}

TEST(ConfigTest, ValidateConfig_InvalidOutputFilename) {
    SAppConfig cfg;
    cfg.output.filename = "";
    EXPECT_FALSE(ValidateConfig(cfg));
}

TEST(ConfigTest, BuildStreamTarget) {
    std::vector<std::string> pairs = {"btcusdt", "ethusdt"};
    std::string target = BuildStreamTarget(pairs);
    EXPECT_NE(target.find("btcusdt"), std::string::npos);
    EXPECT_NE(target.find("ethusdt"), std::string::npos);
}
