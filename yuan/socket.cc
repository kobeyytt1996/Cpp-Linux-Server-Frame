#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>      

#include "fd_manager.h"
#include "hook.h"
#include "iomanager.h"
#include "log.h"
#include "macro.h"
#include "socket.h"

namespace yuan {

static yuan::Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

Socket::ptr Socket::CreateTCP(Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDP(Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateTCPSocket() {
    Socket::ptr sock(new Socket(IPv4, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket() {
    Socket::ptr sock(new Socket(IPv4, UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateTCPSocket6() {
    Socket::ptr sock(new Socket(IPv6, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket6() {
    Socket::ptr sock(new Socket(IPv6, UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixTCPSocket() {
    Socket::ptr sock(new Socket(Unix, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket() {
    Socket::ptr sock(new Socket(Unix, UDP, 0));
    return sock;
}

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
    if (!isValid()) {
        newSock();
        if (YUAN_UNLIKELY(!isValid())) {
            return false;
        }
    }

    if (!addr || addr->getFamily() != getFamily()) {
        YUAN_LOG_ERROR(g_system_logger) << "bind sock.family(" << getFamily() << ") addr.family("
            << addr->getFamily() << ") not equal, addr = " << addr->toString();
        return false;
    }

    if (::bind(m_sockfd, addr->getAddr(), addr->getAddrLen())) {
        YUAN_LOG_ERROR(g_system_logger) << "bind error " << "errno=" << errno
            << "strerr=" << strerror(errno);
        return false;
    }
    getLocalAddress();
    return true;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
    if (!isValid()) {
        newSock();
        if (YUAN_UNLIKELY(!isValid())) {
            return false;
        }
    }

    if (!addr || addr->getFamily() != getFamily()) {
        YUAN_LOG_ERROR(g_system_logger) << "connect sock.family(" << getFamily() << ") addr.family("
            << addr->getFamily() << ") not equal, addr = " << addr->toString();
        return false;
    }

    // 没有指定timeout_ms，则用系统默认约定的超时时间项
    if (timeout_ms == static_cast<uint64_t>(-1)) {
        if (::connect(m_sockfd, addr->getAddr(), addr->getAddrLen())) {
            YUAN_LOG_ERROR(g_system_logger) << "sock=" << m_sockfd << " connect(" << addr->toString()
                << ") error errno=" << errno << " strerr=" << strerror(errno);
            // 连接超时，要记得关闭socket
            close();
            return false;
        }
    } else {
        if (::connect_with_timeout(m_sockfd, addr->getAddr(), addr->getAddrLen(), timeout_ms)) {
            YUAN_LOG_ERROR(g_system_logger) << "sock=" << m_sockfd << " connect(" << addr->toString()
                << ") timeout=" << timeout_ms << "error errno=" << errno << " strerr=" << strerror(errno);
            close();
            return false;
        }
    }

    m_isConnected = true;
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog) {
    if (!isValid()) {
        // 说明bind都没有调用好
        YUAN_LOG_ERROR(g_system_logger) << "listen error sockfd = -1";
        return false;
    }
    if (::listen(m_sockfd, backlog)) {
        YUAN_LOG_ERROR(g_system_logger) << "listen error errno=" << errno << " strerr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::close() {
    if (!m_isConnected && m_sockfd == -1) {
        return true;
    }
    m_isConnected = false;
    if (m_sockfd != -1) {
        ::close(m_sockfd);
        m_sockfd = -1;
    }
    return true;
}

int Socket::send(const void *buffer, size_t length, int flags) {
    if (isConnected()) {
        return ::send(m_sockfd, buffer, length, flags);
    }
    return -1;
}

int Socket::send(const iovec *iov, size_t iovcnt, int flags) {
    if (isConnected()) {
        // 这里实际调用::sendv也可以，但使用了::sendmsg
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = const_cast<iovec*>(iov);
        msg.msg_iovlen = iovcnt;
        return ::sendmsg(m_sockfd, &msg, flags);
    }
    return -1;
}

int Socket::sendTo(const void *buffer, size_t length, const Address::ptr to, int flags) {
    if (isConnected()) {
        return ::sendto(m_sockfd, buffer, length, flags, to->getAddr(), to->getAddrLen());
    }
    return -1;
}

int Socket::sendTo(const iovec *iov, size_t iovcnt, const Address::ptr to, int flags) {
    if (isConnected()) {
        // 这里实际调用::sendv也可以，但使用了::sendmsg
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = const_cast<iovec*>(iov);
        msg.msg_iovlen = iovcnt;
        // sendmsg是下面这样设置接收方地址的
        msg.msg_name = to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg(m_sockfd, &msg, flags);
    }
    return -1;
}

int Socket::recv(void *buffer, size_t length, int flags) {
    if (isConnected()) {
        return ::recv(m_sockfd, buffer, length, flags);
    }
    return -1;
}

int Socket::recv(iovec *iov, size_t iovcnt, int flags) {
    if (isConnected()) {
        // 这里实际调用::sendv也可以，但使用了::sendmsg
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = const_cast<iovec*>(iov);
        msg.msg_iovlen = iovcnt;
        return ::recvmsg(m_sockfd, &msg, flags);
    }
    return -1;
}

int Socket::recvFrom(void *buffer, size_t length, const Address::ptr from, int flags) {
    if (isConnected()) {
        socklen_t len = from->getAddrLen();
        return ::recvfrom(m_sockfd, buffer, length, flags, from->getAddr(), &len);
    }
    return -1;
}

int Socket::recvFrom(iovec *iov, size_t iovcnt, const Address::ptr from, int flags) {
    if (isConnected()) {
        // 这里实际调用::sendv也可以，但使用了::sendmsg
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = const_cast<iovec*>(iov);
        msg.msg_iovlen = iovcnt;
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg(m_sockfd, &msg, flags);
    }
    return -1;
}


Address::ptr Socket::getRemoteAddress() {
    if (m_remoteAddress) {
        return m_remoteAddress;
    }

    Address::ptr result;
    switch (m_family) {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
            break;
    }

    // 为UnixAddress准备的，需要存下来地址长度
    socklen_t addr_len = result->getAddrLen();
    if (getpeername(m_sockfd, result->getAddr(), &addr_len)) {
        YUAN_LOG_ERROR(g_system_logger) << "getpeername error sock=" << m_sockfd
            << " errno=" << errno << " strerr=" << strerror(errno);
        return Address::ptr(new UnknownAddress(m_family));
    }

    if (m_family == AF_UNIX) {
        UnixAddress::ptr unix_addr = std::dynamic_pointer_cast<UnixAddress>(result);
        unix_addr->setAddrLen(addr_len);
    }
    m_remoteAddress = result;
    return m_remoteAddress;
}


Address::ptr Socket::getLocalAddress() {
    if (m_localAddress) {
        return m_localAddress;
    }

    // 细节：不直接在m_localAddress上操作，防止中间出现错误无法保持原子性
    Address::ptr result;
    switch (m_family) {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
            break;
    }

    // 为UnixAddress准备的，需要存下来地址长度
    socklen_t addr_len = result->getAddrLen();
    if (getsockname(m_sockfd, result->getAddr(), &addr_len)) {
        YUAN_LOG_ERROR(g_system_logger) << "getsockname error sock=" << m_sockfd
            << " errno=" << errno << " strerr=" << strerror(errno);
        return Address::ptr(new UnknownAddress(m_family));
    }

    if (m_family == AF_UNIX) {
        UnixAddress::ptr unix_addr = std::dynamic_pointer_cast<UnixAddress>(result);
        unix_addr->setAddrLen(addr_len);
    }
    m_localAddress = result;
    return m_localAddress;
}

bool Socket::isValid() const {
    return m_sockfd != -1;
}

int Socket::getError() {
    int error = 0;
    if (!getOption(SOL_SOCKET, SO_ERROR, error)) {
        return -1;
    }
    return error;
}

std::ostream &Socket::dump(std::ostream &os) const {
    os << "[Socket sockfd=" << m_sockfd 
        << " is_connected=" << m_isConnected
        << " family=" << m_family
        << " type=" << m_type
        << " protocol=" << m_protocol;
    if (m_localAddress) {
        os << " local_address=" << m_localAddress->toString();
    }
    if (m_remoteAddress) {
        os << " remote_address=" << m_remoteAddress->toString();
    }
    os << "]";
    return os;
}

bool Socket::cancelRead() {
    return IOManager::GetThis()->cancelEvent(m_sockfd, IOManager::READ);
}

bool Socket::cancelWrite() {
    return IOManager::GetThis()->cancelEvent(m_sockfd, IOManager::WRITE);
}

bool Socket::cancelAccept() {
    return IOManager::GetThis()->cancelEvent(m_sockfd, IOManager::READ);
}

bool Socket::cancelAll() {
    return IOManager::GetThis()->cancelAll(m_sockfd);
}

bool Socket::init(int sockfd) {
    FdCtx::ptr fd_ctx = FdMgr::GetInstance()->get(sockfd);
    if (fd_ctx && fd_ctx->isSocket() && !fd_ctx->isClosed()) {
        m_sockfd = sockfd;
        m_isConnected = true;
        initSockOption();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}

void Socket::initSockOption() {
    int val = 1;
    // TODO；在bind后调用还是否有用？
    setOption(SOL_SOCKET, SO_REUSEADDR, val);
    if (m_type == SOCK_STREAM) {
        // 关闭Nagle算法，降低延迟：https://blog.csdn.net/lclwjl/article/details/80154565
        setOption(IPPROTO_TCP, TCP_NODELAY, val);
    }
}

void Socket::newSock() {
    m_sockfd = socket(m_family, m_type, m_protocol);
    if (YUAN_LIKELY(m_sockfd != -1)) {
        initSockOption();
    } else {
        YUAN_LOG_ERROR(g_system_logger) << "socket(" << m_family << ", " << m_type
             << ", " << m_protocol << ") errno = " << errno << " errstr = " << strerror(errno);
    }
}

std::ostream &operator<<(std::ostream &os, const Socket &sock) {
    return sock.dump(os);
}


}