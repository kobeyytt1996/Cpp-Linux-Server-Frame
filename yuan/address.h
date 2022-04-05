#ifndef __YUAN_ADDRESS_H__
#define __YUAN_ADDRESS_H__

/**
 * 封装三种socket address的类，IPv4，IPv6和UNIXAddress（靠进程间文件传输，不走协议栈，效率更高）
 * OOP思想，把C的struct封装起来，使得各种操作更好用，如本地字节序和网络字节序的转换、获取网卡地址、解析域名等等
 */

#include <arpa/inet.h>
#include <iostream>
#include <memory>
#include <map>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <vector>

namespace yuan {

class IPAddress;

// 要被各种地址子类继承的基类
class Address {
public:
    typedef std::shared_ptr<Address> ptr;
    // 通用构造方法
    static Address::ptr Create(const sockaddr *addr, socklen_t addrlen);
    // 根据域名获取地址对象，以下几个参数可以对照getaddrinfo的传入参数。results为传出参数
    // host格式：域名:服务名。根据服务名可以查到公知端口号。域名可以常规域名，也可以是[IPv6]，也可以是IPv4的点分十进制
    // 可能会返回多个重复的相同IP地址，因为它们的传输层socktype和protocol是不同的。getaddrinfo都会返回。
    static bool Lookup(std::vector<Address::ptr> &results, const std::string &host
        , int family = AF_INET, int socktype = 0, int protocol = 0);
    // 同上面的Lookup，只是返回值为所有符合条件中的一个
    static Address::ptr LookupAny(const std::string &host
        , int family = AF_INET, int socktype = 0, int protocol = 0);
    // 同上面的Lookup，只是返回值为所有符合条件中的一个IP类型的地址
    static std::shared_ptr<IPAddress> LookupAnyIPAdress(const std::string &host
        , int family = AF_INET, int socktype = 0, int protocol = 0);

    // 获取所有网卡地址。通过getifaddrs的库方法来实现。result里的uint32_t是子网掩码中1的bit数
    static bool GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>> &result_map
        ,int family = AF_UNSPEC);
    // 上面方法的重载，根据网卡名字获取网卡地址
    static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>> &result_vec
        , const std::string &iface, int family = AF_UNSPEC);

    virtual ~Address() {}
    int getFamily() const;

    virtual const sockaddr *getAddr() const = 0;
    virtual sockaddr *getAddr() = 0;
    virtual socklen_t getAddrLen() const = 0;

    virtual std::ostream &print(std::ostream &os) const = 0;
    std::string toString() const;

    bool operator<(const Address &rhs) const;
    bool operator==(const Address &rhs) const;
    bool operator!=(const Address &rhs) const;

};

class IPAddress : public Address {
public:
    typedef std::shared_ptr<IPAddress> ptr;
    // 根据明文IP地址转换对应的IP对象。可能是IPv4的点分十进制法，也可以是IPv6的表示方法
    static IPAddress::ptr Create(const std::string &address, uint16_t port = 0);

    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

    // 因为后续涉及到大小端字节序的转换，这里必须给uint16_t，如果给更大范围，转换之后会有问题
    virtual uint16_t getPort() const = 0;
    virtual void setPort(uint16_t port) = 0;
};

class IPv4Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv4Address> ptr;
    // 根据文本型的IPv4地址（点分十进制法）创建IPv4对象
    static IPv4Address::ptr Create(const std::string &address, uint16_t port);

    IPv4Address(const sockaddr_in &addr);
    // 这里也包含了IPv4Address的默认构造方法
    IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

    const sockaddr *getAddr() const override;
    sockaddr *getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream &print(std::ostream &os) const override;

    // 获取广播地址。prefix_len的单位是bit
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    // 获取子网网段
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint16_t getPort() const override;
    void setPort(uint16_t port) override;

private:
    sockaddr_in m_addr;
};

class IPv6Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv6Address> ptr;
    // 根据文本型的IPv6地址创建IPv6对象
    static IPv6Address::ptr Create(const std::string &address, uint16_t port = 0);

    IPv6Address();
    IPv6Address(const sockaddr_in6 &addr);
    IPv6Address(const uint8_t address[16], uint16_t port = 0);

    const sockaddr *getAddr() const override;
    sockaddr *getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream &print(std::ostream &os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint16_t getPort() const override;
    void setPort(uint16_t port) override;

private:
    sockaddr_in6 m_addr;
};

class UnixAddress : public Address {
public:
    typedef std::shared_ptr<UnixAddress> ptr;
    UnixAddress();
    UnixAddress(const std::string &path);

    const sockaddr *getAddr() const override;
    sockaddr *getAddr() override;
    socklen_t getAddrLen() const override;
    void setAddrLen(socklen_t len) { m_length = len; }
    std::ostream &print(std::ostream &os) const override;

private:
    sockaddr_un m_addr;
    // 和上面两个IP地址不一样。额外需要一个成员变量记录上面地址结构体的最大长度
    socklen_t m_length;
};

class UnknownAddress : public Address {
public:
    typedef std::shared_ptr<UnknownAddress> ptr;
    UnknownAddress(int family);
    UnknownAddress(const sockaddr &addr);

    const sockaddr *getAddr() const override;
    sockaddr *getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream &print(std::ostream &os) const override;

private:
    sockaddr m_addr;
};

std::ostream &operator<<(std::ostream &os, const Address &addr);

}

#endif