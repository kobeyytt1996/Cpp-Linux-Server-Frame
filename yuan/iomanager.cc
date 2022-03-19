#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include "macro.h"
#include "iomanager.h"
#include "log.h"
#include <string.h>

namespace yuan {

static Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

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
    if (fd < m_fdContexts.size()) {
        fd_ctx = m_fdContexts[fd];
        readLock.unlock();
    } else {
        // 必须解开读锁，因为后面resize要加写锁了
        readLock.unlock();
        resizeFdContexts(fd + 1);
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
    if (fd >= m_fdContexts.size()) {
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
    if (fd >= m_fdContexts.size()) {
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
    FdContext::EventContext &event_ctx = fd_ctx->getEventContext(event);
    // 触发事件的方法会清除掉这些事件，外面不需要处理
    fd_ctx->triggerEvent(event_ctx);
    --m_pendingEventCount;
    
    return true;
}

bool IOManager::cancelAll(int fd) {
    RWMutexType::ReadLock readLock(m_mutex);
    if (fd >= m_fdContexts.size()) {
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
        FdContext::EventContext &event_ctx = fd_ctx->getEventContext(IOManager::READ);
        fd_ctx->triggerEvent(event_ctx);
        --m_pendingEventCount;
    }
    if (fd_ctx->events & IOManager::WRITE) {
        FdContext::EventContext &event_ctx = fd_ctx->getEventContext(IOManager::WRITE);
        fd_ctx->triggerEvent(event_ctx);
        --m_pendingEventCount;
    }
    
    return true;
}

IOManager *IOManager::GetThis() {
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {

}
bool IOManager::stopping() {

}

void IOManager::idle() {

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