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
        // 肯定比之前的下次执行时间晚，所以不需要考虑插入到set最前面的情况
        m_manager->m_timers.insert(shared_from_this());
        return true;
    }
    return false;
}

bool Timer::reset(uint64_t ms, bool from_now) {
    if (ms == m_ms && !from_now) {
        return true;
    } 
    TimerManager::RWMutexType::WriteLock write_lock(m_manager->m_mutex);
    if (m_cb) {
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end()) {
            return false;
        }

        m_manager->m_timers.erase(it);

        if (from_now) {
            m_next = yuan::GetCurrentTimeMS() + ms;
        } else {
            m_next = m_next - m_ms + ms;
        }
        m_ms = ms;
        
        m_manager->addTimer(shared_from_this(), write_lock);
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
    m_previousTime = yuan::GetCurrentTimeMS();
}

TimerManager::~TimerManager() {
    
}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    RWMutexType::WriteLock write_lock(m_mutex);
    addTimer(timer, write_lock);

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
    m_tickled = false;
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

    bool rollover = detectClockRollover(now_ms);
    if (!rollover && now_ms < (*m_timers.begin())->m_next) {
        return;
    }

    // 借以下方式调用set的upper_bound
    Timer::ptr now_timer(new Timer(now_ms));
    // 服务器时间如果被调了，则先简单粗暴的把所有Timer都移除
    auto it = rollover ? m_timers.end() : m_timers.upper_bound(now_timer);
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

void TimerManager::addTimer(Timer::ptr timer, RWMutexType::WriteLock &write_lock) {
    auto it = m_timers.insert(timer).first;
    bool at_front = (it == m_timers.begin()) && (!m_tickled);
    if (at_front) {
        m_tickled = true;
    }
    write_lock.unlock();

    if (at_front) {
        onTimerInsertedAtFront();
    }
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
    bool rollover = false;
    // 这里只是个经验值，比之前的校准时间还小一个小时，肯定是有问题的
    if (now_ms < m_previousTime - 60 * 60 * 1000) {
        rollover = true;
    }
    m_previousTime = now_ms;
    return rollover;
}




}