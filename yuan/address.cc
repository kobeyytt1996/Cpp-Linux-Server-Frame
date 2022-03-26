#include "address.h"
#include "endian.h"

#include <algorithm>
#include <sstream>

namespace yuan {

// 获取子网掩码位全0，主机位全1的模板方法。对返回值取~就是子网掩码
template<typename T>
static T CreateMask(uint32_t bits) {
    return (1 << (sizeof(T) * 8 - bits)) - 1;
}

/**
 * Address的实现
 */
int Address::getFamily() const {
    return getAddr()->sa_family;
}

std::string Address::toString() const {
    std::stringstream ss;
    print(ss);
    return ss.str();
}

bool Address::operator<(const Address &rhs) const {
    socklen_t min_len = std::min(getAddrLen(), rhs.getAddrLen());
    int result = memcmp(getAddr(), rhs.getAddr(), min_len);

    if (result < 0) {
        return true;
    } else if (result > 0) {
        return false;
    } else if (getAddrLen() < rhs.getAddrLen()) {
        return true;
    }
    return false;
}

bool Address::operator==(const Address &rhs) const {
    return getAddrLen() == rhs.getAddrLen() 
        && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address &rhs) const {
    return !(*this == rhs);
}

/**
 * IPv4Address的方法实现
 */
IPv4Address::IPv4Address(const sockaddr_in &addr) : m_addr(addr) {}

IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
    m_addr.sin_port = byteswapOnLittleEndian(port);
}

const sockaddr *IPv4Address::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&m_addr);
}

socklen_t IPv4Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream &IPv4Address::print(std::ostream &os) const {
    // 取出来的是网络字节序的ip地址
    uint32_t ip_addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
    os << ((ip_addr >> 24) & 0xff) << '.'
        << ((ip_addr >> 16) & 0xff) << '.'
        << ((ip_addr >> 8) & 0xff) << '.'
        << ((ip_addr >> 8) & 0xff);
    // TODO:这里是否应转成小端字节序
    os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
    return os;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
    if (prefix_len > 32) {
        return nullptr;
    }

    sockaddr_in b_addr(m_addr);
    b_addr.sin_addr.s_addr |= byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));

    return IPv4Address::ptr(new IPv4Address(b_addr));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
    if (prefix_len > 32) {
        return nullptr;
    }

    sockaddr_in n_addr(m_addr);
    // 和获取广播地址的区别就在这里
    n_addr.sin_addr.s_addr &= ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));

    return IPv4Address::ptr(new IPv4Address(n_addr));
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
    if (prefix_len > 32) {
        return nullptr;
    }

    sockaddr_in sm_addr;
    memset(&sm_addr, 0, sizeof(sm_addr));
    sm_addr.sin_addr.s_addr = ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(sm_addr));
}

uint16_t IPv4Address::getPort() const {
    return byteswapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t port) {
    m_addr.sin_port = byteswapOnLittleEndian(port);
}

/**
 * IPv6Address的方法实现
 */
IPv6Address::IPv6Address() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6 &addr) : m_addr(addr) { }

IPv6Address::IPv6Address(const char *address, uint16_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
    m_addr.sin6_port = byteswapOnLittleEndian(port);
}

const sockaddr *IPv6Address::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&m_addr);
}

socklen_t IPv6Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream &IPv6Address::print(std::ostream &os) const {
    // 重点：ipv6的字符串表示方法：https://docs.oracle.com/cd/E19253-01/819-7058/ipv6-overview-24/index.html
    os << '[';
    // s6_addr是大小为16的uint8_t数组，想处理为大小为8的uint16_t数组
    const uint16_t *ip6_addr = reinterpret_cast<const uint16_t *>(&m_addr.sin6_addr.s6_addr);
    bool used_zeros = false;
    // 重点看下面的处理逻辑
    for (size_t i = 0; i < 8; ++i) {
        if (ip6_addr[i] == 0 && !used_zeros) {
            continue;
        }
        if (i > 0 && ip6_addr[i - 1] == 0 && !used_zeros) {
            os << ":";
            used_zeros = true;
        }
        if (i > 0) {
            os << ":";
        }
        os << std::hex << static_cast<int>(byteswapOnLittleEndian(ip6_addr[i])) << std::dec;
    }

    if (!used_zeros && ip6_addr[7] == 0) {
        os << "::";
    }

    os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
    return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
    // IPv6的广播地址也一样，即主机段的位（这里是prefix_len后的位）全都为1：https://docs.oracle.com/cd/E19253-01/819-7058/ipv6-overview-123/index.html
    sockaddr_in6 b_addr(m_addr);
    // 先处理不被8整除的情况，再把后面的也都置为1
    b_addr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint8_t>(prefix_len % 8);
    for (int i = prefix_len / 8 + 1; i < 16; ++i) {
        b_addr.sin6_addr.s6_addr[i] |= 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(b_addr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
    sockaddr_in6 n_addr(m_addr);
    // 先处理不被8整除的情况，再把后面的也都置为1
    n_addr.sin6_addr.s6_addr[prefix_len / 8] &= ~CreateMask<uint8_t>(prefix_len % 8);
    for (int i = prefix_len / 8 + 1; i < 16; ++i) {
        n_addr.sin6_addr.s6_addr[i] = 0x00;
    }
    return IPv6Address::ptr(new IPv6Address(n_addr));
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;

    subnet.sin6_addr.s6_addr[prefix_len / 8] = ~CreateMask<uint8_t>(prefix_len % 8);
    for (int i = 0; i < prefix_len / 8; ++i) {
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(subnet));
}

uint16_t IPv6Address::getPort() const {
    return byteswapOnLittleEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t port) {
    m_addr.sin6_port = byteswapOnLittleEndian(port);
}

/**
 * UnixAddress
 */
static const size_t MAX_PATH_LEN = sizeof(sockaddr_un::sun_path) - 1;

UnixAddress::UnixAddress() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    // 注意：offset的使用
    m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string &path) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    // +1是因为后面memcpy的时候取c_str，后面会补一个'\0'
    m_length = path.size() + 1;
    
    // 处理一下传入地址第一个字符为空字符的特殊情况
    if (!path.empty() && path[0] == '\0') {
        --m_length;
    }

    if (m_length > sizeof(m_addr.sun_path)) {
        throw std::logic_error("path too long");
    }
    memcpy(m_addr.sun_path, path.c_str(), m_length);
    m_length += offsetof(sockaddr_un, sun_path);
}

const sockaddr *UnixAddress::getAddr() const {
    return reinterpret_cast<const sockaddr*>(&m_addr);
}

socklen_t UnixAddress::getAddrLen() const {
    return m_length;
}

std::ostream &UnixAddress::print(std::ostream &os) const {
    if (m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
        return os << "\\0" << std::string(m_addr.sun_path + 1
            , m_length - offsetof(sockaddr_un, sun_path) - 1);
    }
    return os << m_addr.sun_path;
}

/**
 * UnknownAddress
 */
UnknownAddress::UnknownAddress(int family) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sa_family = family;
}

const sockaddr *UnknownAddress::getAddr() const {
    return &m_addr;
}

socklen_t UnknownAddress::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream &UnknownAddress::print(std::ostream &os) const {
    os << "[UnknownAddress family=" << m_addr.sa_family << "]";
    return os;
}

}