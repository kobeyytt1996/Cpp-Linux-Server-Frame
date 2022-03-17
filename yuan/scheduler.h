#ifndef __YUAN_SCHEDULER_H__
#define __YUAN_SCHEDULER_H__
/**
 * @brief 调度器：scheduler ---> thread ---> fiber。依然沿用每个Thread上有个主协程的模式。
 * 既是线程池，能分配一组线程
 * 也是协程调度器，将协程指定到相应的线程上去执行
 * 在每个线程上，有Scheduler的主协程执行run方法。如果是Scheduler创建的线程，线程主协程和Scheduler在该线程的主协程是同一个
 * 如果是创建Scheduler的线程，线程主协程和Scheduler在该线程的主协程是不同的。注意区分
 */
#include <memory>
#include "fiber.h"
#include "thread.h"
#include <functional>
#include <list>
#include <vector>
#include <map>
#include <atomic>

namespace yuan {

// 调度基类。以后还要继承扩展，比如和epoll结合的调度器
class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    // use_caller指是否把调用此构造方法的线程也加入线程池管理，name是调度器（线程池）的名字
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "");
    virtual ~Scheduler();

    const std::string getName() const { return m_name; }

    // 核心方法。开启调度器
    void start();
    // 核心方法。不能直接退出调度器，最好等所有任务都运行完才退出
    void stop();

    // 调度方法。模板类是因为既能传function也能传fiber。
    template<typename FiberOrCb>
    void schedule(FiberOrCb foc, int thread = -1) {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNoLock(foc, thread);
        }

        if (need_tickle) {
            tickle();
        }
    }

    // 批量调度方法。泛型的设计思维。批量增加的好处是能保证任务在消息队列中的顺序
    template<typename FiberOrCbIterator>
    void schedule(FiberOrCbIterator &begin, FiberOrCbIterator &end) {
        bool need_tickle = false;
        while (begin != end) {
            MutexType::Lock lock(m_mutex);
            need_tickle = (scheduleNoLock(&*begin, -1) || need_tickle);
            ++begin;
        }

        if (need_tickle) { 
            tickle();
        }
    }

protected:
    // 通知唤醒的方法
    virtual void tickle();
    // 核心方法：真正在执行协程调度的方法。协调线程和协程的关系。
    void run();
    // stop的时候子类可能需要做一些其它回收资源的事情
    virtual bool stopping();
    // 没任务做时，执行idle。具体由子类实现。可以占住cpu，也可以sleep。e.g. idle里调用epoll_wait
    virtual void idle();

    // 把当前线程的scheduler设为自己
    void setThis();
public:
    static Scheduler *GetThis();
    // 获取当前线程的调度器主协程（运行run方法的协程）
    static Fiber *GetMainFiber();

private:
    // 封装fiber和function作为可执行的对象
    struct FiberAndThread {
        Fiber::ptr fiber;
        std::function<void()> cb;
        // 记录要在那个Thread上执行该任务
        int threadId;

        FiberAndThread(Fiber::ptr f, int thr) : fiber(f), threadId(thr) {}
        // 细节：这个构造方法让传入的智能指针变为空，减少了引用计数，控制权转给Scheduler
        FiberAndThread(Fiber::ptr *f, int thr) : threadId(thr) {
            f->swap(fiber);
        }

        FiberAndThread(std::function<void()> f, int thr) : cb(f), threadId(thr) {}

        FiberAndThread(std::function<void()> *f, int thr) : threadId(thr) {
            f->swap(cb);
        }
        // 要放在stl里，所以要有默认构造函数来初始化
        FiberAndThread() : threadId(-1) {}

        void reset() {
            fiber = nullptr;
            cb = nullptr;
            threadId = -1;
        }
    };

private:
    // 不加锁的调度方法。模板类是因为既能传function也能传fiber。
    template<typename FiberOrCb>
    bool scheduleNoLock(FiberOrCb foc, int thread) {
        // 如果m_fibers为空，则可能所有线程在阻塞态，需要通知唤醒，从协程队列取出任务
        bool need_tickle = m_fibers.empty();
        FiberAndThread task(foc, thread);
        if (task.cb || task.fiber) {
            m_fibers.push_back(std::move(task));
        }
        return need_tickle;
    }

private:
    MutexType m_mutex;
    // 线程池
    std::vector<Thread::ptr> m_threads;
    // 计划（即将）执行的对象的集合(队列)，可以是协程，也可以是function
    std::list<FiberAndThread> m_fibers;
    // 如果use_caller为true，该主线程里的主协程已被使用。需要scheduler自己准备一个该线程里的主协程
    Fiber::ptr m_rootFiber; 
    std::string m_name;

protected:
    // 以下是为了便于扩展的属性变量
    // 所有线程ID的集合。用户可以调度任务时可以不传一个明确已有的线程ID，比如传100，可以对线程总数取模得到执行线程
    std::vector<int> m_threadIds;
    // 总线程数
    size_t m_threadCount = 0;
    // 在执行任务队列里的任务的线程数量。使用原子量保证线程安全
    std::atomic<size_t> m_activeThreadCount = {0};
    // 在空闲等待的线程数量
    std::atomic<size_t> m_idleThreadCount = {0};
    // 调度器的运行状态。创建出来默认是停止的
    bool m_stopping = true;
    // 标志着是否已被要求关闭（调用stop），等任务执行完后关闭
    bool m_autoStop = false;
    // 调用调度器构造函数的主线程ID
    int m_rootThreadId = 0;
};

}

#endif