// Tests for CTradeQueue
#include <gtest/gtest.h>
#include "trade_queue.hpp"
#include "trade.hpp"
#include <thread>

TEST(TradeQueueTest, PushPop) {
    CTradeQueue queue;
    STrade trade{"BTCUSDT", 100.0, 1.0, 123456, true};
    queue.Push(trade);
    STrade out;
    ASSERT_TRUE(queue.Pop(out));
    EXPECT_EQ(out.symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(out.price, 100.0);
    EXPECT_DOUBLE_EQ(out.quantity, 1.0);
    EXPECT_EQ(out.timestamp, 123456u);
    EXPECT_TRUE(out.buyer_initiated);
}

TEST(TradeQueueTest, PopEmpty) {
    CTradeQueue queue;
    STrade out;
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        queue.Stop();
    });
    ASSERT_FALSE(queue.Pop(out));
    t.join();
}

TEST(TradeQueueTest, Stop) {
    CTradeQueue queue;
    queue.Stop();
    STrade out;
    ASSERT_FALSE(queue.Pop(out));
}

TEST(TradeQueueTest, PushAfterStop) {
    CTradeQueue queue;
    queue.Stop();
    STrade trade{"BTCUSDT", 100.0, 1.0, 123456, true};
    queue.Push(trade);
    STrade out;
    ASSERT_FALSE(queue.Pop(out));
}

TEST(TradeQueueTest, MultiThread) {
    CTradeQueue queue;
    const int N = 10;
    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            queue.Push({"BTCUSDT", 100.0 + i, 1.0, static_cast<uint64_t>(1000 + i), true});
        }
        queue.Stop();
    });
    int count = 0;
    STrade out;
    while (queue.Pop(out)) {
        ++count;
    }
    producer.join();
    ASSERT_EQ(count, N);
}
