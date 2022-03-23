#include "fd_manager.h"
#include "hook.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace yuan {

/**
 * FdCtx的方法实现
 */
FdCtx::FdCtx(int fd) 
    : m_isInit(false) 
    , m_isSocket(false) 
    , m_sysNonBlock(false)
    , m_userNonBlock(false)
    , m_isClosed(false)
    , m_fd(fd)
    , m_recvTimeout(-1)
    , m_sendTimeout(-1) {
        init();
}

FdCtx::~FdCtx() {
}

bool FdCtx::init() {
    if (m_isInit) {
        return true;
    }
    m_recvTimeout = -1;
    m_sendTimeout = -1;

    struct stat fd_stat;
    // 使用fstat获取是否是socket
    if (fstat(m_fd, &fd_stat) == -1) {
        m_isInit = false;
        m_isSocket = false;
    } else {
        m_isInit = true;
        m_isSocket = S_ISSOCK(fd_stat.st_mode);
    }

    // 确保socket的fd都被设置为非阻塞
    if (m_isSocket) {
        // 这里一定要确保用没hook的系统的fcntl，故使用fcntl_f。
        int flags = fcntl_f(m_fd, F_GETFL);
        if (!(flags & O_NONBLOCK)) {
            fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        m_sysNonBlock = true;
    } else {
        m_sysNonBlock = false;
    }

    m_userNonBlock = false;
    m_isClosed = false;
    return m_isInit;
}

bool FdCtx::close() {
    return true;
}

void FdCtx::setSysNonBlock(bool flag) {

}

void FdCtx::setUserNonBlock(bool flag) {

}

uint64_t FdCtx::getTimeout(int type) const {
    if (type == SO_RCVTIMEO) {
        return m_recvTimeout;
    } else {
        return m_sendTimeout;
    }
}

void FdCtx::setTimeout(int type, uint64_t timeout) {
    if (type == SO_RCVTIMEO) {
        m_recvTimeout = timeout;
    } else {
        m_sendTimeout = timeout;
    }
}

/**
 * FdManager的方法实现
 */
FdManager::FdManager() {
    m_datas.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create) {
    RWMutexType::ReadLock read_lock(m_mutex);
    if (static_cast<int>(m_datas.size()) <= fd) {
        if (!auto_create) {
            return nullptr;
        } else {
            m_datas.resize(fd * 1.5);
        }
    } else {
        if (!auto_create || m_datas[fd]) {
            return m_datas[fd];
        }
    }
    read_lock.unlock();

    FdCtx::ptr fd_ctx(new FdCtx(fd));
    RWMutexType::WriteLock write_lock(m_mutex);
    m_datas[fd] = fd_ctx;
    return fd_ctx;
}

void FdManager::del(int fd) {
    RWMutexType::WriteLock write_lock(m_mutex);
    if (static_cast<int>(m_datas.size()) <= fd) {
        return;
    }
    m_datas[fd].reset();
}

}