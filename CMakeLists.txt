cmake_minimum_required(VERSION 3.15)
project(cqg LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Find packages installed via Conan
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(GTest REQUIRED)

# 1. Main application
add_executable(cqg 
    src/main.cpp 
    src/websocket_client.cpp 
    src/websocket_client.hpp 
    src/app_runner.cpp
    src/app_runner.hpp
    src/config.cpp
    src/config.hpp
    src/trade.cpp
    src/trade.hpp
    src/trade_queue.cpp
    src/trade_queue.hpp 
    src/aggregator.cpp
    src/aggregator.hpp
    src/logger.cpp
    src/logger.hpp
)

target_link_libraries(cqg 
    PRIVATE 
    Boost::system 
    OpenSSL::SSL 
    nlohmann_json::nlohmann_json
)

# 2. Unit tests

# 2. Unit tests
add_executable(unit_tests 
    tests/test_aggregator.cpp
    tests/test_trade_queue.cpp
    tests/test_config.cpp
    tests/test_websocket_client.cpp
    tests/test_app_runner.cpp
    tests/test_trade.cpp
    src/aggregator.cpp
    src/trade.cpp
    src/trade_queue.cpp
    src/logger.cpp
    src/config.cpp
    src/websocket_client.cpp
    src/app_runner.cpp
    src/aggregator.hpp
    src/trade.hpp
    src/trade_queue.hpp
    src/logger.hpp
    src/config.hpp
    src/websocket_client.hpp
    src/app_runner.hpp
)

target_link_libraries(unit_tests 
    PRIVATE 
    GTest::gtest_main
    GTest::gtest
    GTest::gmock
    Boost::system 
    OpenSSL::SSL 
    nlohmann_json::nlohmann_json
)

target_include_directories(unit_tests PRIVATE src)

add_test(NAME unit_tests COMMAND unit_tests)