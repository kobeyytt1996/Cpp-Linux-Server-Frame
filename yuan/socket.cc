#include <netinet/tcp.h>

#include "fd_manager.h"
#include "log.h"
#include "macro.h"
#include "socket.h"

namespace yuan {

static yuan::Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

Socket::Socket(int family, int type, int protocol)
    : m_sockfd(-1)
    , m_family(family)
    , m_type(type)
    , m_protocol(protocol)
    , m_isConnected(false) {}

Socket::~Socket() {
    // OOP的好处之一，确保close socket
    close();
}

int64_t Socket::getSendTimeout() const {
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(m_sockfd);
    if (fd_ctx) {
        return fd_ctx->getTimeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeout(int64_t timeout_ms) {
    // 细节：聚合类可以直接用initializer_list的方式构造
    struct timeval tv{int(timeout_ms / 1000), int(timeout_ms % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout() const {
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(m_sockfd);
    if (fd_ctx) {
        return fd_ctx->getTimeout(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeout(int64_t timeout_ms) {
    struct timeval tv{int(timeout_ms / 1000), int(timeout_ms % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void *result, size_t *len) {
    // 交给hook过的系统函数处理
    int ret = getsockopt(m_sockfd, level, option, result, (socklen_t*)len);
    if (ret) {
        YUAN_LOG_DEBUG(g_system_logger) << "getOption sockfd = " << m_sockfd
            << " level = " << level << " option = " << option 
            << " errno = " << errno << "strerr = " << strerror(errno);
        return false;
    } else {
        return true;
    }
}

bool Socket::setOption(int level, int option, const void *value, size_t len) {
    if (setsockopt(m_sockfd, level, option, value, static_cast<socklen_t>(len))) {
        YUAN_LOG_DEBUG(g_system_logger) << "setOption sockfd = " << m_sockfd
            << " level = " << level << " option = " << option 
            << " errno = " << errno << "strerr = " << strerror(errno);
        return false;
    }
    return true;
}

Socket::ptr Socket::accept() {
    Socket::ptr new_sock(new Socket(m_family, m_type, m_protocol));
    // 细节：注意使用的是全局的accept
    int new_sockfd = ::accept(m_sockfd, nullptr, nullptr);
    if (new_sockfd == -1) {
        YUAN_LOG_ERROR(g_system_logger) << "accept(" << m_sockfd << ") errno = "
            << errno << " strerror = " << strerror(errno);
        return nullptr;
    }
    if (new_sock->init(new_sockfd)) {
        return new_sock;
    }
    return nullptr;
}

bool Socket::bind(const Address::ptr addr) {

}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {

}

bool Socket::listen(int backlog = SOMAXCONN) {

}

bool Socket::close() {

}

int Socket::send(const void *buffer, size_t length, int flags) {

}

int Socket::send(const iovec *iov, size_t iovcnt, int flags) {

}

int Socket::sendTo(const void *buffer, size_t length, const Address::ptr to, int flags) {

}

int Socket::sendTo(const iovec *iov, size_t iovcnt, const Address::ptr to, int flags) {

}

int Socket::recv(void *buffer, size_t length, int flags = 0) {
    
}

int Socket::recv(iovec *iov, size_t iovcnt, int flags = 0) {

}

int Socket::recvFrom(void *buffer, size_t length, const Address::ptr from, int flags) {

}

int Socket::recvFrom(iovec *iov, size_t iovcnt, const Address::ptr from, int flags) {

}


Address::ptr Socket::getRemoteAddress() const {

}


Address::ptr Socket::getLocalAddress() const {

}

bool Socket::isValid() const {

}

int Socket::getError() const {

}

std::ostream &Socket::dump(std::ostream &os) const {

}

bool Socket::cancelRead() {

}

bool Socket::cancelWrite() {

}

bool Socket::cancelAccept() {

}

bool Socket::cancelAll() {

}

bool Socket::init(int sockfd) {
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(sockfd);
    if (fd_ctx && fd_ctx->isSocket() && !fd_ctx->isClosed()) {
        m_sockfd = sockfd;
        m_isConnected = true;
        initSock();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}

void Socket::initSock() {
    int val = 1;
    // TODO；在bind后调用还是否有用？
    setOption(SOL_SOCKET, SO_REUSEADDR, val);
    if (m_type == SOCK_STREAM) {
        // 关闭Nagle算法：https://blog.csdn.net/lclwjl/article/details/80154565
        setOption(SOL_SOCKET, TCP_NODELAY, val);
    }
}

void Socket::newSock() {
    m_sockfd = socket(m_family, m_type, m_protocol);
    if (YUAN_LIKELY(m_sockfd != -1)) {
        initSock();
    } else {
        YUAN_LOG_ERROR(g_system_logger) << "socket(" << m_family << ", " << m_type
             << ", " << m_protocol << ") errno = " << errno << " errstr = " << strerror(errno);
    }
}

}