#ifndef __YUAN_IOMANAGER_H__
#define __YUAN_IOMANAGER_H__
/**
 * IOManager是Scheduler的子类，负责IO协程调度，底层用epoll实现
 * 
 */

#include "scheduler.h"
#include <sys/epoll.h>

namespace yuan {

class IOManager : public Scheduler {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;

    enum Event {
        NONE = 0x0,
        READ = EPOLLIN,
        WRITE = EPOLLOUT
    };

public:
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
        // 清除（重置事件）
        void resetEventContext(EventContext &ctx);
        // 强制触发事件，执行后重置
        void triggerEvent(Event event);
    };

public:
    IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "");
    ~IOManager() override;

    // 使用epoll_ctl开始监听指定fd上的指定事件，并且该事件触发后，还可调用callback。返回值：0:success, -1:error
    int addEvent(int fd, Event event, std::function<void()> cb);
    bool delEvent(int fd, Event event);
    // 和删除事件的区别在于，找到事件后，强制执行
    bool cancelEvent(int fd, Event event);
    // 取消一个fd上所有事件。和cancel的取消方式相同
    bool cancelAll(int fd);

    static IOManager *GetThis();

protected:
    void tickle() override;
    bool stopping() override;
    void idle() override;

    // resize m_fdContexts。并且给所有空指针都new对象，这样有点浪费空间，但addEvent等函数里面就可以加读锁即可。空间换时间
    void resizeFdContexts(size_t size);

private:
    // 用于epoll的fd
    int m_epfd = 0;
    // 管道用于统一事件源。epoll_wait时，消息队列里有新任务时，要通过管道唤醒
    int m_tickleFds[2];

    std::atomic<size_t> m_pendingEventCount = {0};
    RWMutexType m_mutex;
    // 为了便于查找，下标即fd大小。空间换时间
    std::vector<FdContext*> m_fdContexts;
};

}

#endif