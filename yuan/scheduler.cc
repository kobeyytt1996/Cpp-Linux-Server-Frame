#include "scheduler.h"
#include "log.h"
#include "macro.h"

namespace yuan {

static yuan::Logger::ptr g_logger = YUAN_GET_LOGGER("system");

// 分别是线程的调度器和线程层面的主协程
static thread_local Scheduler *t_scheduler = nullptr;
static thread_local Fiber *t_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
    : m_name(name) {
    YUAN_ASSERT(threads > 0);

    if (use_caller) {
        // 一个线程上只能有一个调度器。先判断是否已经初始化过。
        YUAN_ASSERT(GetThis() == nullptr);

        // 能创建线程层面的主协程
        Fiber::GetThis();
        t_scheduler = this;
        --threads;

        // 重点：因为use_caller，线程层面的主协程不能参与run方法，新开协程来运行schedule的协程调度方法。注意bind的使用
        m_rootFiber.reset(new Fiber(std::bind(Scheduler::run, this)));
        t_fiber = m_rootFiber.get();

        Thread::SetName(m_name);
        m_rootThreadId = GetThreadId();
        m_threadIds.push_back(m_rootThreadId);
    } else {
        m_rootThreadId = -1;
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler() {
    YUAN_ASSERT(m_stopping)
    if (GetThis() == this) {
        t_scheduler = nullptr;
    }
}

void Scheduler::start() {
    MutexType::Lock lock(m_mutex);
    if (!m_stopping) {
        return;
    }
    m_stopping = false;

    YUAN_ASSERT(m_threads.empty());
    m_threads.resize(m_threadCount);
    for (int i = 0; i < m_threads.size(); ++i) {
        m_threads[i].reset(new Thread(std::bind(Scheduler::run, this), m_name + "_" + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }
}

void Scheduler::stop() {
    m_autoStop = true;
    // if为use_caller且只有一个线程的情况
    if (m_rootFiber && m_threadCount == 0
        && (m_rootFiber->getState() == Fiber::TERM || m_rootFiber->getState() == Fiber::EXCEPT
            || m_rootFiber->getState() == Fiber::INIT)) {
        YUAN_LOG_INFO(g_logger) << this << " stopped";
        m_stopping = true;

        if (stopping()) {
            return;
        }
    }

    bool exit_on_this_fiber = false;
    if (m_rootThreadId != -1) {
        // 如果使用了创建scheduler的线程。stop一定要在该线程上执行
        YUAN_ASSERT(GetThis() == this);      
    } else {
        // 没使用。stop可以在任意没有使用自己这个scheduler的线程上执行
        YUAN_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    for (int i = 0; i < m_threadCount; ++i) {
        // 多少个线程就唤醒多少次。唤醒后线程自己结束自己
        tickle();
    }

    if (m_rootFiber) {
        // 针对每一个调run方法的地方，都要调用tickle
        tickle();
    }

    if (stopping()) {
        return;
    }

    // if (exit_on_this_fiber) {
    // }
}

void Scheduler::tickle() {

}

void Scheduler::run() {
    setThis();
    // 如果不是主线程，则设置线程层面的主协程。如果是主线程，构造函数已设置过t_fiber
    if (GetThreadId() != m_rootThreadId) {
        t_fiber = Fiber::GetThis().get();
    }

    Fiber::ptr idle_fiber(new Fiber());
}

bool Scheduler::stopping() {

}

void Scheduler::setThis() {
    t_scheduler = this;
}

Scheduler *Scheduler::GetThis() {
    return t_scheduler;
}

Fiber *Scheduler::GetMainFiber() {
    return t_fiber;
}
}