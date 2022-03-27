#include "address.h"
#include "endian.h"
#include "log.h"

#include <algorithm>
#include <netdb.h>
#include <sstream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace yuan {

static yuan::Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

// 获取子网掩码位全0，主机位全1的模板方法。对返回值取~就是子网掩码
template<typename T>
static T CreateMask(uint32_t bits) {
    return (1 << (sizeof(T) * 8 - bits)) - 1;
}

/**
 * Address的实现
 */

Address::ptr Address::Create(const sockaddr *addr, socklen_t addrlen) {
    if (!addr) {
        return nullptr;
    }

    Address::ptr result;
    switch (addr->sa_family) {
        case AF_INET:
            result.reset(new IPv4Address(*reinterpret_cast<const sockaddr_in*>(addr)));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*reinterpret_cast<const sockaddr_in6*>(addr)));
            break;
        default:
            result.reset(new UnknownAddress(*addr));
            break;
    }
    return result;
}

bool Address::Lookup(std::vector<Address::ptr> &results_vec, const std::string &host
        , int family = AF_UNSPEC, int socktype = 0, int protocol = 0) {
    addrinfo hints;
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_protocol = protocol;
    hints.ai_addr = 0;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_next = nullptr;

    std::string node;
    const char *service;

    // 以下开始分情况解析host。先解析[IPv6]:服务名的格式
    if (!host.empty() && host[0] == '[') {
        const char *endipv6 = static_cast<const char*>(memchr(host.c_str(), ']', host.size()));
        if (endipv6) {
            // TODO:加上越界检查
            if (*(endipv6 + 1) == ':') {
                service = endipv6 + 2;
            }
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }

    // [域名|IPv4的点分十进制]:服务名
    if (node.empty()) {
        service = static_cast<const char*>(memchr(host.c_str(), ':', host.size())) + 1;
        if (service) {
            if (!memchr(service, ':', host.c_str() + host.size() - service - 1)) {
                node = host.substr(0, service - host.c_str() - 1);
            }
        }
    }
    // 不属于前两种
    if (node.empty()) {
        node = host;
    }

    addrinfo *results;
    int ret = getaddrinfo(node.c_str(), service, &hints, &results);
    if (ret) {
        YUAN_LOG_ERROR(g_system_logger) << "Address::Lookup getaddrinfo(" << host << ", "
            << family << ", " << socktype << ", " << protocol << ") err = " << ret 
            << " errstr = " << strerror(errno);
        return false;
    }

    // 遍历存下所有返回的符合条件的sockaddr
    addrinfo *next = results;
    while (next) {
        results_vec.push_back(Address::Create(next->ai_addr, next->ai_addrlen));
        next = next->ai_next;
    }
    freeaddrinfo(results);
    return true;
}

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
 * IPAddress的方法实现
 */

IPAddress::ptr IPAddress::Create(const std::string &address, uint16_t port) {
    // 根据getaddrinfo的库方法实现，man查看，比gethostbyname更好，且是线程安全的。且功能很强大
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    // 强调AI_NUMERICHOST，即不查询域名，只做明文的IP地址解析
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;

    addrinfo *results;
    int ret = getaddrinfo(address.c_str(), NULL, &hints, &results);
    if (ret) {
        YUAN_LOG_ERROR(g_system_logger) << "IPAddress::Create(" << address << ", "
            << port << ") error = " << ret << " errno = " << errno << " errstr = " << strerror(errno);
        return nullptr;
    }

    try {
        // 有可能是IPv4、IPv6，也有可能是Unknown，后者的话这里会转为nullptr
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
            Address::Create(results->ai_addr, results->ai_addrlen));
        if (result) {
            result->setPort(port);
        }
        freeaddrinfo(results);
        return result;
    } catch(...) {
        freeaddrinfo(results);
        return nullptr;
    }
}

/**
 * IPv4Address的方法实现
 */

IPv4Address::ptr IPv4Address::Create(const std::string &address, uint16_t port) {
    IPv4Address::ptr ret(new IPv4Address);
    ret->m_addr.sin_port = byteswapOnLittleEndian(port);
    int result = inet_pton(AF_INET, address.c_str(), &ret->m_addr.sin_addr.s_addr);
    // man inet_pton看返回值的含义
    if (result <= 0) {
        YUAN_LOG_ERROR(g_system_logger) << "IPv4Address::Create(" << address << ", "
            << port << ") ret = " << result << " errno = " << errno
            << " errstr = " << strerror(errno);
        return nullptr;
    }

    return ret;
}

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

IPv6Address::ptr IPv6Address::Create(const std::string &address, uint16_t port = 0) {
    IPv6Address::ptr ret(new IPv6Address);
    ret->m_addr.sin6_port = byteswapOnLittleEndian(port);
    int result = inet_pton(AF_INET6, address.c_str(), &ret->m_addr.sin6_addr.s6_addr);
    // man inet_pton看返回值的含义
    if (result <= 0) {
        YUAN_LOG_ERROR(g_system_logger) << "IPv6Address::Create(" << address << ", "
            << port << ") ret = " << result << " errno = " << errno
            << " errstr = " << strerror(errno);
        return nullptr;
    }

    return ret;
}

IPv6Address::IPv6Address() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6 &addr) : m_addr(addr) { }

IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
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
    // 也可以man inet_pton查看
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
    for (uint32_t i = prefix_len / 8 + 1; i < 16; ++i) {
        n_addr.sin6_addr.s6_addr[i] = 0x00;
    }
    return IPv6Address::ptr(new IPv6Address(n_addr));
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;

    subnet.sin6_addr.s6_addr[prefix_len / 8] = ~CreateMask<uint8_t>(prefix_len % 8);
    for (uint32_t i = 0; i < prefix_len / 8; ++i) {
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

UnknownAddress::UnknownAddress(const sockaddr &addr) : m_addr(addr) {}

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