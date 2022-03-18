#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include "macro.h"
#include "iomanager.h"
#include "log.h"

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
    // 之前已在该fd上添加过相同事件
    if (fd_ctx->events & event) {

    }
}

bool IOManager::delEvent(int fd, Event event) {

}

bool IOManager::cancelEvent(int fd, Event event) {

}

bool IOManager::cancelAll(int fd) {

}

IOManager *IOManager::GetThis() {

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