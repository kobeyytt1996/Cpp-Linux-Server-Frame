#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "macro.h"
#include "iomanager.h"
#include "log.h"

namespace yuan {

static Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

/**
 * @brief 下面几个是IOManager里EventContext的几个方法的实现
 */
IOManager::FdContext::EventContext &IOManager::FdContext::getEventContext(IOManager::Event event) {
    switch (event) {
        case IOManager::READ:
            return readEventContext;
        case IOManager::WRITE:
            return writeEventContext;
        default:
            YUAN_ASSERT2(false, "getEventContext");
    }
}
       
void IOManager::FdContext::resetEventContext(EventContext &ctx) {
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}
     
void IOManager::FdContext::triggerEvent(IOManager::Event event) {
    YUAN_ASSERT(event & events);
    EventContext &event_ctx = getEventContext(event);
    YUAN_ASSERT(event_ctx.scheduler);
    if (event_ctx.cb) {
        // 细节：注意下面这两个实参都要加&，直接swap进去，让event_ctx.cb和fiber都变为空指针
        event_ctx.scheduler->schedule(&event_ctx.cb);
    } else if (event_ctx.fiber) {
        event_ctx.scheduler->schedule(&event_ctx.fiber);
    }
    
    resetEventContext(event_ctx);
    events = static_cast<IOManager::Event>(events & (~event));
    return;
}

/**
 * @brief 下面几个是IOManager的方法实现
 */
IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
    : Scheduler(threads, use_caller, name) {

    m_epfd = epoll_create(1);
    YUAN_ASSERT(m_epfd > 0);

    int ret = pipe(m_tickleFds); 
    YUAN_ASSERT(!ret);

    epoll_event event;
    memset(&event, 0, sizeof(event));
    // 边沿触发
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0];

    ret = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    YUAN_ASSERT(!ret);

    ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    YUAN_ASSERT(!ret);

    resizeFdContexts(64);
    // 默认创建即运行IO调度器
    start();
}

IOManager::~IOManager() {
    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for (decltype(m_fdContexts.size()) i = 0; i < m_fdContexts.size(); ++i) {
        if (m_fdContexts[i]) {
            delete m_fdContexts[i];
        }
    }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    FdContext *fd_ctx = nullptr;
    // 下面的读写锁是针对m_fdContexts的
    RWMutexType::ReadLock readLock(m_mutex);
    if (static_cast<size_t>(fd) < m_fdContexts.size()) {
        fd_ctx = m_fdContexts[fd];
        readLock.unlock();
    } else {
        // 必须解开读锁，因为后面resize要加写锁了
        readLock.unlock();
        // 按fd的1.5倍进行扩容，防止频繁扩容
        resizeFdContexts(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }

    // 这是针对取出的FdContext加的锁
    FdContext::MutexType::Lock fdContextLock(fd_ctx->mutex);
    // 之前已在该fd上添加过相同事件。说明有两个线程要在同一个fd上监听同一个事件
    if (fd_ctx->events & event) {
        YUAN_LOG_ERROR(g_system_logger) << "addEvent assert fd=" << fd 
            << " event=" << event << " fd_ctx.event=" << fd_ctx->events;
        YUAN_ASSERT(!(fd_ctx->events & event));
    }

    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->events | event;
    // 注意这里要存fd_ctx，方便之后取用
    epevent.data.ptr = fd_ctx;

    int ret = epoll_ctl(m_epfd, op, fd_ctx->fd, &epevent);
    if (ret) {
        // 注意如何尽可能的输出错误信息
        YUAN_LOG_ERROR(g_system_logger) << "epoll_ctl(" << m_epfd << ", " << op << ","
            << fd << "," << epevent.events << "): " << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return -1;
    }

    ++m_pendingEventCount;
    fd_ctx->events = static_cast<IOManager::Event>(fd_ctx->events | event);
    FdContext::EventContext &event_ctx = fd_ctx->getEventContext(event);
    YUAN_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);

    event_ctx.scheduler = Scheduler::GetThis();
    if (cb) {
        event_ctx.cb.swap(cb);
    } else {
        event_ctx.fiber = Fiber::GetThis();
        YUAN_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
    }
    return 0;
}

bool IOManager::delEvent(int fd, Event event) {
    RWMutexType::ReadLock readLock(m_mutex);
    if (static_cast<size_t>(fd) >= m_fdContexts.size()) {
        return false;
    }
    FdContext *fd_ctx = m_fdContexts[fd];
    readLock.unlock();

    FdContext::MutexType::Lock fdContextLock(fd_ctx->mutex);
    if (!(event & fd_ctx->events)) {
        return false;
    }

    Event new_events = static_cast<IOManager::Event>(fd_ctx->events & (~event));
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    // EPOLLET别忽略
    epevent.events = new_events | EPOLLET;
    epevent.data.ptr = fd_ctx;

    int ret = epoll_ctl(m_epfd, op, fd_ctx->fd, &epevent);
    if (ret) {
        YUAN_LOG_ERROR(g_system_logger) << "epoll_ctl(" << m_epfd << ", " << op << ","
            << fd << "," << epevent.events << "): " << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
    }

    --m_pendingEventCount;
    fd_ctx->events = new_events;
    FdContext::EventContext &event_ctx = fd_ctx->getEventContext(event);
    fd_ctx->resetEventContext(event_ctx);
    
    return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
    // 以下代码都和delEvent一样，除了下面注释那几行
    RWMutexType::ReadLock readLock(m_mutex);
    if (static_cast<size_t>(fd) >= m_fdContexts.size()) {
        return false;
    }
    FdContext *fd_ctx = m_fdContexts[fd];
    readLock.unlock();

    FdContext::MutexType::Lock fdContextLock(fd_ctx->mutex);
    if (!(event & fd_ctx->events)) {
        return false;
    }

    Event new_events = static_cast<IOManager::Event>(fd_ctx->events & (~event));
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    // EPOLLET别忽略
    epevent.events = new_events | EPOLLET;
    epevent.data.ptr = fd_ctx;

    int ret = epoll_ctl(m_epfd, op, fd_ctx->fd, &epevent);
    if (ret) {
        YUAN_LOG_ERROR(g_system_logger) << "epoll_ctl(" << m_epfd << ", " << op << ","
            << fd << "," << epevent.events << "): " << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
    }

    // 与delEvent的区别，要强行触发
    // 触发事件的方法会清除掉这些事件，外面不需要处理
    fd_ctx->triggerEvent(event);
    --m_pendingEventCount;
    
    return true;
}

bool IOManager::cancelAll(int fd) {
    RWMutexType::ReadLock readLock(m_mutex);
    if (static_cast<size_t>(fd) >= m_fdContexts.size()) {
        return false;
    }
    FdContext *fd_ctx = m_fdContexts[fd];
    readLock.unlock();

    FdContext::MutexType::Lock fdContextLock(fd_ctx->mutex);
    if (!fd_ctx->events) {
        return false;
    }

    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    // EPOLLET别忽略
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int ret = epoll_ctl(m_epfd, op, fd_ctx->fd, &epevent);
    if (ret) {
        YUAN_LOG_ERROR(g_system_logger) << "epoll_ctl(" << m_epfd << ", " << op << ","
            << fd << "," << epevent.events << "): " << ret << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
    }

    if (fd_ctx->events & IOManager::READ) {
        fd_ctx->triggerEvent(IOManager::READ);
        --m_pendingEventCount;
    }
    if (fd_ctx->events & IOManager::WRITE) {
        fd_ctx->triggerEvent(IOManager::WRITE);
        --m_pendingEventCount;
    }
    
    return true;
}

IOManager *IOManager::GetThis() {
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
    // 如果没有空闲线程，说明没有线程在epoll_wait，则不写入消息唤醒在空闲等待的线程
    if (!hasIdleThreads()) {
        return;
    }
    int ret = write(m_tickleFds[1], "T", 1);
    YUAN_ASSERT(ret == 1);
}

bool IOManager::stopping() {
    return m_pendingEventCount == 0 && Scheduler::stopping();
}

void IOManager::idle() {
    // epoll_wait需要数组，故不能用stl
    epoll_event *epevents = new epoll_event[64]();
    // 技巧：shared_ptr不能直接管理数组，可以用以下方式
    std::shared_ptr<epoll_event> events_ptr(epevents, [](epoll_event *ep_event){
        delete [] ep_event;
    });

    while (true) {
        // 距最近的定时器执行还有多长时间
        uint64_t next_timeout = getNextTimer();
        if (stopping()) {
            if (next_timeout == UINT64_MAX) {
                YUAN_LOG_INFO(g_system_logger) << "name =" << getName() << " idle stopping exit";
                break;
            }
        }

        int ret = 0;
        do {
            // epoll_wait的单位为ms
            static const int MAX_TIMEOUT = 1000;
            if (next_timeout == UINT64_MAX) {
                next_timeout = MAX_TIMEOUT;
            } else {
                next_timeout = std::min(static_cast<int>(next_timeout), MAX_TIMEOUT);
            }
            ret = epoll_wait(m_epfd, epevents, 64, static_cast<int>(next_timeout));
            if (ret < 0 && errno == EINTR) {
            } else {
                break;
            }
        } while (true);

        // 先处理epoll_wait唤醒是因为有定时任务的情况
        std::vector<std::function<void()>> timer_cbs;
        listExpiredCbs(timer_cbs);
        if (!timer_cbs.empty()) {
            schedule(timer_cbs.begin(), timer_cbs.end());
            timer_cbs.clear();
        }

        for (int i = 0; i < ret; ++i) {
            epoll_event &ep_event = epevents[i];
            // 先检查是否是被tickle唤醒的
            if (ep_event.data.fd == m_tickleFds[0]) {
                uint8_t dummy;
                // 可能有多个其他线程都tickle了，把管道中的数据都取出来。但这些数据没有用，仅仅是通知
                while (read(m_tickleFds[0], &dummy, 1) == 1);
                continue;
            }

            if (ep_event.events & (EPOLLERR | EPOLLHUP)) {
                ep_event.events |= (EPOLLIN | EPOLLOUT);
            }
            int real_events = NONE;
            if (ep_event.events & EPOLLIN) {
                real_events |= READ;
            }
            if (ep_event.events & EPOLLOUT) {
                real_events |= WRITE;
            }

            FdContext *fd_ctx = static_cast<FdContext*>(ep_event.data.ptr);
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
            if ((fd_ctx->events & real_events) == NONE) {
                continue;
            }

            int left_events = fd_ctx->events & (~real_events);
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            ep_event.events = EPOLLET | left_events;

            int ret2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &ep_event);
            if (ret2) {
                YUAN_LOG_ERROR(g_system_logger) << "epoll_ctl(" << m_epfd << ", " << op << ","
                    << fd_ctx->fd << "," << ep_event.events << "): " << ret 
                    << " (" << errno << ") (" << strerror(errno) << ")";
                    continue;
            }

            if (real_events & READ) {
                fd_ctx->triggerEvent(READ);
                --m_pendingEventCount;
            }
            if (real_events & WRITE) {
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }

        // 每次epoll_wait被唤醒并触发完事件后，要让出执行权给Scheduler的主协程的run方法。
        // 下面尽量不要用智能指针，要不可能会影响idleFiber的生命周期。（swapout后栈上一直保存着智能指针）
        Fiber *raw_ptr = Fiber::GetThis().get();
        raw_ptr->swapOut();
    }
}

void IOManager::onTimerInsertedAtFront() {
    // 先把epoll_wait唤醒
    tickle();
}

void IOManager::resizeFdContexts(size_t size) {
    RWMutexType::WriteLock writeLock(m_mutex);
    m_fdContexts.resize(size);
    for (size_t i = 0; i < m_fdContexts.size(); ++i) {
        if (!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext();
        }
        m_fdContexts[i]->fd = i;
    }
}

}