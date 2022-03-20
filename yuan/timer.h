#ifndef __YUAN_TIMER_H__
#define __YUAN_TIMER_H__

#include <memory>
#include <functional>
#include <set>
#include "thread.h"

namespace yuan {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;
public:
    typedef std::shared_ptr<Timer> ptr;
private:
    // 构造方法为私有，只有在TimerManager里可以构建Timer。ms是执行周期，recurring是是否循环执行
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager);

private:
    // 执行周期。epoll只支持ms，故用ms即可
    uint64_t m_ms = 0;
    // 下次到时的精确时间
    uint64_t m_next = 0;
    // 是否是循环定时器
    bool m_recurring = false;
    TimerManager *m_manager = nullptr;
    std::function<void()> m_cb;
private:
    // 关键：必须提供能够比较Timer的规则
    struct Comparator {
        bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
    };
};

// 可能被各种Scheduler继承来获得管理定时器的方法
class TimerManager {
friend class Timer;
public:
    typedef RWMutex RWMutexType;

    TimerManager();
    // 基类的析构一定是virtual
    virtual ~TimerManager();

    // 添加定时器，周期性做一些事情
    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);
    // 条件定时器，满足weak_cond所指不为空才执行
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb
        , std::weak_ptr<void> weak_cond, bool recurring = false);

protected:
    // 如果新的计时器插入到了最前端，说明之前epoll_wait要等待的时间可能长了，
    // 因此要留给iomanager实现，来唤醒epoll_wait，重新调整时间
    virtual void onTimerInsertedAtFront() = 0;
private:
    RWMutexType m_mutex;
    // 重点：利用了set红黑树的有序性，给timer排序
    std::set<Timer::ptr, Timer::Comparator> m_timers;
};

}

#endif
