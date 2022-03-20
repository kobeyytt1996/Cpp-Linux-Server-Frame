#include "timer.h"
#include "util.h"

namespace yuan {

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager)
    :  m_ms(ms), m_recurring(recurring), m_manager(manager), m_cb(cb) {
    m_next = getCurrentTimeMS() + m_ms;
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

TimerManager::TimerManager() {

}

TimerManager::~TimerManager() {
    
}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    RWMutexType::WriteLock write_lock(m_mutex);
    auto it = m_timers.insert(timer).first;
    bool at_front = (it == m_timers.begin());

    if (at_front) {
        onTimerInsertedAtFront();
    }

    return timer;
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb
        , std::weak_ptr<void> weak_cond, bool recurring = false) {
    
}

}