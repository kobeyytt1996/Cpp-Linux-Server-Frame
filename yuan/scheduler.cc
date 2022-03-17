#include "scheduler.h"
#include "log.h"
#include "macro.h"

namespace yuan {

static yuan::Logger::ptr g_logger = YUAN_GET_LOGGER("system");

// 线程的调度器
static thread_local Scheduler *t_scheduler = nullptr;
// 每个线程执行Scheduler::run方法的主协程
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

        // 重点：因为use_caller，线程层面的主协程不能参与run方法，新开协程来运行schedule的协程调度方法。
        // 注意bind的使用。成员函数前必须加取址符号
        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
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
    // 要加block，否则下面m_rootFiber运行run的时候会出现死锁
    {
        MutexType::Lock lock(m_mutex);
        if (!m_stopping) {
            return;
        }
        m_stopping = false;

        YUAN_ASSERT(m_threads.empty());
        m_threads.resize(m_threadCount);
        for (decltype(m_threads.size()) i = 0; i < m_threads.size(); ++i) {
            // 各个线程都执行run方法，作为主协程代码，然后在里面切换协程，调度任务
            m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
            m_threadIds.push_back(m_threads[i]->getId());
        }
    }

    // 下面的代码不能放在start里，因为目前的实现中从线程的主协程拿过控制权后没有归还，除非run结束。start后的schedule无法及时执行到。故先放在stop里
    // use_caller为true，在主线程中的m_rootFiber也要开始执行，才能运行给它设置的run方法
    // if (m_rootFiber) {
        // 从线程的主协程拿过控制权
    //     m_rootFiber->call();
    //     YUAN_LOG_INFO(g_logger) << "call out";
    // }
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

    // bool exit_on_this_fiber = false;
    if (m_rootThreadId != -1) {
        // 如果使用了创建scheduler的线程（use_caller为true）。stop一定要在该线程上执行
        YUAN_ASSERT(GetThis() == this);      
    } else {
        // 没使用。stop可以在任意没有使用自己这个scheduler的线程上执行
        YUAN_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    for (decltype(m_threadCount) i = 0; i < m_threadCount; ++i) {
        // 多少个线程就唤醒多少次。唤醒后线程自己结束自己
        tickle();
    }

    if (m_rootFiber) {
        // 针对每一个调run方法的地方，都要调用tickle
        tickle();
    }

    // m_rootFiber执行run放在stop里，原因在start里已说明
    if (m_rootFiber) {
        while (!stopping()) {
            if (m_rootFiber->getState() == Fiber::TERM || m_rootFiber->getState() == Fiber::EXCEPT) {
                m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
            }
            YUAN_LOG_INFO(g_logger) << " scheduler root fiber is term, reset";
            m_rootFiber->call();
        }
    }

    if (stopping()) {
        return;
    }

    // if (exit_on_this_fiber) {
    // }
}

void Scheduler::tickle() {
    YUAN_LOG_INFO(g_logger) << "tickle";
}

void Scheduler::run() {
    YUAN_LOG_INFO(g_logger) << "scheduler run";

    setThis();
    // 如果不是主线程，线程的主协程和调度器在该线程的主协程是相同的。如果是主线程，构造函数已设置过t_fiber
    if (GetThreadId() != m_rootThreadId) {
        t_fiber = Fiber::GetThis().get();
    }

    // 任务队列里没有任务可执行时，执行idle
    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
    // 为下面的任务队列中的function对象准备的协程
    Fiber::ptr cb_fiber;

    FiberAndThread fat;
    while (true) {
        fat.reset();
        // 有可能当前线程并不是想要唤醒的线程，那么当前线程就要接过再唤醒其他线程的任务
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            // 遍历任务队列，取出能在当前线程执行的任务
            auto it = m_fibers.begin();
            while (it != m_fibers.end()) {
                // 有指定要在非当前线程上执行的任务
                if (it->threadId != -1 && it->threadId != GetThreadId()) {
                    need_tickle = true;
                    ++it;
                    continue;
                }

                YUAN_ASSERT(it->fiber || it->cb);
                if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
                    ++it;
                    continue;
                }

                fat = *it;
                m_fibers.erase(it);
                break;
            }
        }

        if (need_tickle) {
            tickle();
        }

        if (fat.fiber && fat.fiber->getState() != Fiber::TERM && fat.fiber->getState() != Fiber::EXCEPT) {
            ++m_activeThreadCount;
            fat.fiber->swapIn();
            --m_activeThreadCount;
            // fat.fiber因某种原因停止了执行，分情况处理
            // 协程里调用了YieldToReady
            if (fat.fiber->getState() == Fiber::READY) {
                schedule(fat.fiber);
            } else if (fat.fiber->getState() != Fiber::TERM && fat.fiber->getState() != Fiber::EXCEPT) {
                fat.fiber->setState(Fiber::HOLD);
                // TODO:化为HOLD状态后，下面fat就要reset了，之后这个协程还有谁能引用到？下面对fat.cb也要类似问题
            }
            // 用完则断开引用，防止不能及时回收
            fat.reset();
        } else if (fat.cb) {
            if (cb_fiber) {
                cb_fiber->reset(fat.cb);
            } else {
                cb_fiber.reset(new Fiber(fat.cb));
            }
            fat.reset();

            ++m_activeThreadCount;
            cb_fiber->swapIn();
            --m_activeThreadCount;

            if (cb_fiber->getState() == Fiber::READY) {
                schedule(cb_fiber);
                // cb_fiber原来指向的对象进入任务队列，因此cb_fiber要指向新的协程
                cb_fiber.reset();
            } else if (cb_fiber->getState() == Fiber::TERM || cb_fiber->getState() == Fiber::EXCEPT) {
                cb_fiber->reset(nullptr);
            } else {
                // 到这里的只有可能是EXEC和HOLD
                cb_fiber->setState(Fiber::HOLD);
                cb_fiber.reset();
            }
        }
        // 没有任务可执行，执行idle_fiber
        else {
            if (idle_fiber->getState() == Fiber::TERM) {
                YUAN_LOG_INFO(g_logger) << "idle fiber term";
                // 先简单粗暴处理：既没有任务，空闲协程也已终止，则整个线程任务完成，跳出while(true)
                break;
            }
            ++m_idleThreadCount;
            idle_fiber->swapIn();
            --m_idleThreadCount;
            if (idle_fiber->getState() != Fiber::TERM && idle_fiber->getState() != Fiber::EXCEPT) {
                idle_fiber->setState(Fiber::HOLD);
            }
        }
    }
}

bool Scheduler::stopping() {
    MutexType::Lock lock(m_mutex);
    return m_autoStop && m_stopping && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle() {
    YUAN_LOG_INFO(g_logger) << "idle";
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