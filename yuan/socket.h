#ifndef __YUAN_SOCKET_H__
#define __YUAN_SOCKET_H__
/**
 * socket的封装类。继续使用OOP思想，简化使用
 */
#include <memory>
#include <sys/uio.h>

#include "address.h"
#include "noncopyable.h"

namespace yuan {

class Socket : public std::enable_shared_from_this<Socket>, public Noncopyable {
public:
    typedef std::shared_ptr<Socket> ptr;
    typedef std::weak_ptr<Socket> weak_ptr;

    enum type {
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM
    };

    enum family {
        IPv4 = AF_INET,
        IPv6 = AF_INET6,
        Unix = AF_UNIX
    };

    static Socket::ptr CreateTCP(Address::ptr address);
    static Socket::ptr CreateUDP(Address::ptr address);

    static Socket::ptr CreateTCPSocket();
    static Socket::ptr CreateUDPSocket();

    static Socket::ptr CreateTCPSocket6();
    static Socket::ptr CreateUDPSocket6();

    static Socket::ptr CreateUnixTCPSocket();
    static Socket::ptr CreateUnixUDPSocket();

    // 初始化成员函数，但并不创建socket文件描述符。后面其他操作时才创建
    Socket(int family, int type, int protocol = 0);
    ~Socket();

    int64_t getSendTimeout() const;
    void setSendTimeout(int64_t timeout_ms);

    int64_t getRecvTimeout() const;
    void setRecvTimeout(int64_t timeout_ms);

    bool getOption(int level, int option, void *result, size_t *len);
    // 技巧：实现即调用上述方法。但让使用者调用起来更方便。
    template<typename T>
    bool getOption(int level, int option, T &result) {
        size_t length = sizeof(T);
        return getOption(level, option, &result, &length);
    }

    bool setOption(int level, int option, const void *value, size_t len);
    // 技巧：实现即调用上述方法。但让使用者调用起来更方便。
    template<typename T>
    bool setOption(int level, int option, const T &value) {
        return setOption(level, option, &value, sizeof(value));
    }

    Socket::ptr accept();

    bool bind(const Address::ptr addr);
    bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
    // 看man listen了解看默认参数
    bool listen(int backlog = SOMAXCONN);
    bool close();

    int send(const void *buffer, size_t length, int flags = 0);
    // iovec的发送方式
    int send(const iovec *iov, size_t iovcnt, int flags = 0);
    // UDP的发送
    int sendTo(const void *buffer, size_t length, const Address::ptr to, int flags = 0);
    int sendTo(const iovec *iov, size_t iovcnt, const Address::ptr to, int flags = 0);

    int recv(void *buffer, size_t length, int flags = 0);
    // iovec的接收方式
    int recv(iovec *iov, size_t iovcnt, int flags = 0);
    // UDP的接收
    int recvFrom(void *buffer, size_t length, const Address::ptr from, int flags = 0);
    int recvFrom(iovec *iov, size_t iovcnt, const Address::ptr from, int flags = 0);

    // 该socket连接的远端address。该方法会调用getpeername初始该address
    Address::ptr getRemoteAddress();
    // 该socket对应的本地address。该方法会调用getsockname初始该address
    Address::ptr getLocalAddress();

    // 获取对应的fd
    int getSocketFd() const { return m_sockfd; }
    int getFamily() const { return m_family; }
    int getType() const { return m_type; }
    int getProtocol() const { return m_protocol; }
    bool isConnected() const { return m_isConnected; }

    // 判断是否有有效的sockfd
    bool isValid() const;
    // 通过getsockopt获取socket是否有error
    int getError();

    std::ostream &dump(std::ostream &os) const;

    // 下面这些方法都是阻塞的，所以增加相应的取消方法。由iomanager来取消
    bool cancelRead();
    bool cancelWrite();
    bool cancelAccept();
    bool cancelAll();

private:
    // 根据已生成的fd来初始化Socket对象
    bool init(int sockfd);
    // 初始化设定一些选项
    void initSockOption();
    // 新建一个sockfd，是实际调用socket构造方法的地方
    void newSock();

private:
    int m_sockfd;
    int m_family;
    int m_type;
    int m_protocol;
    bool m_isConnected;

    Address::ptr m_remoteAddress;
    Address::ptr m_localAddress;
};

}

#endif