#include "trade.hpp"
#include <chrono>
#include <stdexcept>

bool STrade::IsValid() const {
    return !symbol.empty() && price > 0.0 && quantity > 0.0 &&
           std::isfinite(price) && std::isfinite(quantity) &&
           timestamp > 0;
}

STrade STrade::FromJson(const std::string& raw_data) {
    auto j = nlohmann::json::parse(raw_data);
    if (j.contains("data")) {
        j = j["data"];
    }
    STrade t{
        j["s"].get<std::string>(),
        std::stod(j["p"].get<std::string>()),
        std::stod(j["q"].get<std::string>()),
        j["T"].get<uint64_t>(),
        j["m"].get<bool>()
    };
    if (!t.IsValid()) {
        throw std::runtime_error("Invalid trade data");
    }
    return t;
}
