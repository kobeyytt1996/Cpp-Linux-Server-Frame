#ifndef __YUAN_SCHEDULER_H__
#define __YUAN_SCHEDULER_H__
/**
 * @brief 调度器：scheduler ---> thread ---> fiber
 * 既是线程池，能分配一组线程
 * 也是协程调度器，将协程指定到相应的线程上去执行
 */
#include <memory>
#include "fiber.h"
#include "thread.h"
#include <functional>
#include <list>

namespace yuan {

class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    // use_caller指是否把调用此构造方法的线程也加入线程池管理，name是调度器（线程池）的名字
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "");
    virtual ~Scheduler();

    const std::string getName() const { return m_name; }

    void start();
    void stop();

public:
    static Scheduler *GetThis();
    // 调度器需要一个主协程来安排其他协程
    static Fiber *GetMainFiber();

private:
    // 封装fiber和function作为可执行的对象
    struct FiberAndThread {
        Fiber::ptr fiber;
        std::function<void()> cb;
        // 记录要在那个Thread上执行该任务
        int threadId;

        FiberAndThread(Fiber::ptr f, int thr) : fiber(f), threadId(thr) {}
        // 细节：这个构造方法让传入的智能指针变为空，减少了引用计数，控制器转给Scheduler
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
    // 模板类是因为既能传function也能传fiber。不加锁的调度方法
    template<typename FiberOrCb>
    bool scheduleNoLock(FiberOrCb foc, int thread) {
        // 如果m_fibers为空，则可能所有线程在阻塞态，需要通知唤醒
        bool need_notify = m_fibers.empty();
        FiberAndThread task(foc, thread);
        if (task.cb || task.fiber) {
            m_fibers.push_back(std::move(task));
        }
        return need_notify;
    }

private:
    MutexType m_mutex;
    // 线程池
    std::vector<Thread> m_threads;
    // 计划（即将）执行的对象的集合，可以是协程，也可以是function
    std::list<FiberAndThread> m_fibers; 
    std::string m_name;
};

}

#endif