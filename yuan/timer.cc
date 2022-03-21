#include "timer.h"
#include "util.h"

namespace yuan {

/**
 * 以下是Timer的函数实现
 */

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager)
    :  m_ms(ms), m_recurring(recurring), m_manager(manager), m_cb(cb) {
    m_next = GetCurrentTimeMS() + m_ms;
}

Timer::Timer(uint64_t next) : m_next(next) {

}

bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock writeLock(m_manager->m_mutex);
    if (m_cb) {
        // 减小引用计数
        m_cb = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

bool Timer::refresh() {
    TimerManager::RWMutexType::WriteLock writeLock(m_manager->m_mutex);
    if (m_cb) {
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end()) {
            return false;
        }

        m_manager->m_timers.erase(it);
        m_next = yuan::GetCurrentTimeMS() + m_ms;
        m_manager->m_timers.insert(shared_from_this());
        return true;
    }
    return false;
}

bool Timer::reset(uint64_t ms, bool from_now) {
    TimerManager::RWMutexType::WriteLock writeLock(m_manager->m_mutex);
    if (m_cb) {
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end()) {
            return false;
        }

        m_manager->m_timers.erase(it);
        m_next = yuan::GetCurrentTimeMS() + m_ms;
        m_manager->m_timers.insert(shared_from_this());
        return true;
    }
    return false;
}

bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const {
    // 注意先处理空指针的情况
    if (!lhs) {
        return true;
    }
    if (!rhs) {
        return false;
    }
    if (lhs->m_next < rhs->m_next) {
        return true;
    }
    if (rhs->m_next < lhs->m_next) {
        return false;
    }
    // 下次执行的时间相等，则按虚拟地址排序（无意义）
    return lhs.get() < rhs.get();
}

/**
 * 以下是TimerManager的函数实现
 */

TimerManager::TimerManager() {

}

TimerManager::~TimerManager() {
    
}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    RWMutexType::WriteLock write_lock(m_mutex);
    auto it = m_timers.insert(timer).first;
    bool at_front = (it == m_timers.begin());
    write_lock.unlock();

    if (at_front) {
        onTimerInsertedAtFront();
    }

    return timer;
}

// 技巧：如何使用这个函数辅助addConditionTimer，在想要调用的回调前增加一些代码
static void onTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    std::shared_ptr<void> tmp = weak_cond.lock();
    if (tmp) {
        cb();
    }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb
        , std::weak_ptr<void> weak_cond, bool recurring) {
    return addTimer(ms, std::bind(&onTimer, weak_cond, cb), recurring);
}

uint64_t TimerManager::getNextTimer() {
    RWMutexType::ReadLock readLock(m_mutex);
    if (m_timers.empty()) {
        // 没有定时器，则返回一个最大值
        return UINT64_MAX;
    }

    const Timer::ptr &next = *(m_timers.begin());
    uint64_t now_ms = yuan::GetCurrentTimeMS();
    if (now_ms > next->m_next) {
        // 不知道什么原因，timer已过时但没有执行，返回0
        return 0;
    } else {
        return next->m_next - now_ms;
    }
}

void TimerManager::listExpiredCbs(std::vector<std::function<void()>> &cbs) {
    uint64_t now_ms = yuan::GetCurrentTimeMS();
    std::vector<Timer::ptr> expired;
    
    RWMutexType::ReadLock readLock(m_mutex);
    if (m_timers.empty()) {
        return;
    }
    // 借以下方式调用set的upper_bound
    Timer::ptr now_timer(new Timer(now_ms));
    auto it = m_timers.upper_bound(now_timer);
    expired.insert(expired.begin(), m_timers.begin(), it);
    readLock.unlock();

    RWMutexType::WriteLock writeLock(m_mutex);
    m_timers.erase(m_timers.begin(), it);

    for (auto &timer : expired) {
        cbs.push_back(timer->m_cb);
        if (timer->m_recurring) {
            // 还要放回定时器集合中
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        } else {
            // 减少function的引用计数
            timer->m_cb = nullptr;
        }
    }
}



}