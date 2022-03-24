#ifndef __YUAN_IOMANAGER_H__
#define __YUAN_IOMANAGER_H__
/**
 * IOManager是Scheduler的子类，负责IO协程调度，底层用epoll实现
 * 也是TimerManager的子类，具体定时器的功能
 */

#include "scheduler.h"
#include "timer.h"
#include <sys/epoll.h>

namespace yuan {

class IOManager : public Scheduler, public TimerManager {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;

    enum Event {
        NONE = 0x0,
        // 注意这里的值一定要设置对，后面会把READ、WRITE当作EPOLLIN和EPOLLOUT来使用
        READ = EPOLLIN,
        WRITE = EPOLLOUT
    };

private:
    // 对一个文件描述符相关事件的封装类
    struct FdContext {
        typedef Mutex MutexType;
        struct EventContext {
            // 实际程序中会有多个调度器，指定在哪个调度器中执行
            Scheduler *scheduler;
            // 下面两个是事件的协程和回调函数。二者只有一个会被设置（即事件有两种执行形式，类似Scheduler中的FiberAndThread）
            Fiber::ptr fiber;
            std::function<void()> cb;
        };

        // 读事件和写事件
        EventContext readEventContext;
        EventContext writeEventContext;
        // 事件关联的文件描述符
        int fd = 0;
        // 已经注册的事件
        Event events = NONE;
        MutexType mutex;

        EventContext &getEventContext(Event event);
        // 清除（重置事件）。每次调用该函数，都要记得--m_pendingEventCount
        void resetEventContext(EventContext &ctx);
        // 强制触发事件，执行后重置。每次调用该函数，都要记得--m_pendingEventCount
        void triggerEvent(Event event);
    };

public:
    // 构造函数时默认会start
    IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "");
    // 默认会stop
    ~IOManager() override;

    // 使用epoll_ctl开始监听指定fd上的指定事件，并且该事件触发后，cb有值，则开新协程执行cb。cb为null则唤醒调用addEvent的协程。
    // 返回值：0:success, -1:error
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    bool delEvent(int fd, Event event);
    // 和删除事件的区别在于，找到事件后，强制执行
    bool cancelEvent(int fd, Event event);
    // 取消一个fd上所有事件。和cancel的取消方式相同
    bool cancelAll(int fd);

    static IOManager *GetThis();

protected:
    // 下面三个方法是核心函数，是继承自scheduler
    // 向管道里写入数据以唤醒在idle里epoll_wait的线程
    void tickle() override;
    // 在父类的基础上增加退出条件：监听事件数量为0，没有定时器任务
    bool stopping() override;
    // 核心：空闲时调用epoll_wait，如果有监听的读写事件发生或有tickle，则唤醒，触发事件，并切回Scheduler主协程
    void idle() override;
    // 继承自TimerManager。当插入比之前定时器要执行的事件都更近的定时器的回调
    void onTimerInsertedAtFront() override;

    // resize m_fdContexts。并且给所有空指针都new对象，这样有点浪费空间，但addEvent等函数里面就可以加读锁即可。空间换时间
    void resizeFdContexts(size_t size);
    // 是stopping的实际实现，next_timeout是传入参数，能够获得距离下个定时任务的时间
    bool stopping(uint64_t &next_timeout);
private:
    // 用于epoll的fd
    int m_epfd = 0;
    // 管道用于统一事件源。epoll_wait时，消息队列里有新任务时，调用tickle()，向管道写数据，唤醒epoll_wait
    int m_tickleFds[2];

    // 需要监听的事件个数
    std::atomic<size_t> m_pendingEventCount = {0};
    RWMutexType m_mutex;
    // 为了便于查找，下标即fd大小。空间换时间
    std::vector<FdContext*> m_fdContexts;
};

}

#endif