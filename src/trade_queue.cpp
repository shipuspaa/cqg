#include "trade_queue.hpp"

CTradeQueue::CTradeQueue() : m_stopped(false) {}

void CTradeQueue::Push(STrade trade) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopped) {
            return;
        }
        m_queue.push(std::move(trade));
    }
    m_cond.notify_one();
}

bool CTradeQueue::Pop(STrade& trade) {
    std::unique_lock<std::mutex> lock(m_mutex);

    m_cond.wait(lock, [this]() {
        return !m_queue.empty() || m_stopped;
    });

    if (m_stopped && m_queue.empty()) {
        return false;
    }

    trade = std::move(m_queue.front());
    m_queue.pop();
    return true;
}

void CTradeQueue::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopped = true;
    }
    m_cond.notify_all();
}
