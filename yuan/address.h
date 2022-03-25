#ifndef __YUAN_ADDRESS_H__
#define __YUAN_ADDRESS_H__

/**
 * 封装三种socket address的类，IPv4，IPv6和UNIXAddress（靠进程间文件传输，不走协议栈，效率更高）
 * OOP思想，把C的struct封装起来，使得各种操作更好用，如本地字节序和网络字节序的转换等等
 */

#include <arpa/inet.h>
#include <iostream>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

namespace yuan {

// 要被各种地址子类继承的基类
class Address {
public:
    typedef std::shared_ptr<Address> ptr;

    virtual ~Address() {}
    int getFamily() const;

    virtual const sockaddr *getAddr() const = 0;
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

    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

    // 索然端口号目前最大是65535.但未来没准会扩大，所以给的范围更大
    virtual uint32_t getPort() const = 0;
    virtual void setPort(uint32_t port) = 0;
};

class IPv4Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv4Address> ptr;
    IPv4Address(uint32_t address = INADDR_ANY, uint32_t port = 0);

    const sockaddr *getAddr() const override;
    socklen_t getAddrLen() const override;
    std::ostream &print(std::ostream &os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint32_t port) override;

private:
    sockaddr_in m_addr;
};

class IPv6Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv6Address> ptr;
    IPv6Address(uint32_t address = INADDR_ANY, uint32_t port = 0);

    const sockaddr *getAddr() const override;
    socklen_t getAddrLen() const override;
    std::ostream &print(std::ostream &os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint32_t port) override;

private:
    sockaddr_in6 m_addr;
};

class UnixAddress : public Address {
public:
    typedef std::shared_ptr<UnixAddress> ptr;
    UnixAddress(const std::string &path);

    const sockaddr *getAddr() const override;
    socklen_t getAddrLen() const override;
    std::ostream &print(std::ostream &os) const override;

private:
    sockaddr_un m_addr;
    socklen_t m_length;
};

class UnknownAddress : public Address {
public:
    typedef std::shared_ptr<UnknownAddress> ptr;
    UnknownAddress();

    const sockaddr *getAddr() const override;
    socklen_t getAddrLen() const override;
    std::ostream &print(std::ostream &os) const override;

private:
    sockaddr m_addr;
};

}

#endif