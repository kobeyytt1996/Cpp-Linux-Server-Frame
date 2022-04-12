#ifndef __YUAN_TIMER_H__
#define __YUAN_TIMER_H__

/**
 * @file timer.h
 * 定时器的基础类。具体如何计时由TimerManager的子类来实现。
 * TimerManager可以添加两种定时器：一定执行的和条件的
 */

#include <memory>
#include <functional>
#include <set>
#include <vector>
#include "thread.h"

namespace yuan {

class TimerManager;

// 定时器类。
class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;
public:
    typedef std::shared_ptr<Timer> ptr;

    // 取消定时器。返回值：true则成功取消，false则已经取消过或已经被执行过
    bool cancel();
    // 更新定时器的m_next。返回值：true则成功刷新，false则已经取消过或已经被执行过
    bool refresh();
    // 重新设置ms，from_now指是否从现在算下次要执行的时间
    bool reset(uint64_t ms, bool from_now);
private:
    // 构造方法为私有，只有在TimerManager里可以构建Timer。ms是执行周期，recurring是是否循环执行
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager);
    // 只赋予下次要执行的精确时间。不用来生成真正的Timer，只是设置一个可以来比较的标准
    Timer(uint64_t next);

private:
    // 执行周期。epoll只支持ms，故用ms即可
    uint64_t m_ms = 0;
    // 下次到时的精确时间(时间的绝对值)
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

    // 添加定时器，周期性做一些事情。返回这个定时器，让调用者也能取消定时器等操作
    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);
    // 条件定时器，满足weak_cond所指不为空才执行
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb
        , std::weak_ptr<void> weak_cond, bool recurring = false);

    // 获取距离最近一个定时器要执行的时间
    uint64_t getNextTimer();
    // 把所有已超时但未执行的Timer的cb收集起来
    void listExpiredCbs(std::vector<std::function<void()>> &cbs);
    // 判断是否还有定时器没有执行
    bool hasTimer();

protected:
    // 如果新的计时器插入到了最前端，说明之前epoll_wait要等待的时间可能长了，
    // 因此要留给iomanager实现，来唤醒epoll_wait，重新调整时间
    virtual void onTimerInsertedAtFront() = 0;
    // 一个共有的添加timer到集合中的方法。要处理插到最前面的情况
    void addTimer(Timer::ptr timer, RWMutexType::WriteLock &write_lock);
private:
    // 如果服务器的时间被调了，也要能检测到并做相应调整
    bool detectClockRollover(uint64_t now_ms);
private:
    RWMutexType m_mutex;
    // 重点：利用了set红黑树的有序性，给timer排序
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    // 提高性能：多次连续添加新计时器到最前端，只调用onTimerInsertedAtFront一次。getNextTimer后再置为false
    bool m_tickled = false;
    // 记录设定计时器时的时间，用来校准是否服务器时间有变化
    uint64_t m_previousTime = 0;
};

}

#endif
