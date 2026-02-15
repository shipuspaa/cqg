#pragma once
#include <cmath>
#include <string>

#include <nlohmann/json.hpp>

struct STrade {
    std::string symbol;
    double price;
    double quantity;
    uint64_t timestamp;
    bool buyer_initiated;

    bool IsValid() const;
    static STrade FromJson(const std::string& raw_data);
};