#pragma once

#include "trade.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>

class CTradeQueue {
public:
    CTradeQueue();

    virtual void Push(STrade trade);
    bool Pop(STrade& trade);
    void Stop();

private:
    std::queue<STrade> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_stopped;
};