#include "address.h"
#include "endian.h"

#include <algorithm>
#include <sstream>

namespace yuan {

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
IPv4Address::IPv4Address(uint32_t address, uint32_t port) {
    byteswap(32);
}

const sockaddr *IPv4Address::getAddr() const {

}

socklen_t IPv4Address::getAddrLen() const {

}

std::ostream &IPv4Address::print(std::ostream &os) const {

}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {

}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {

}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {

}

uint32_t IPv4Address::getPort() const {

}

void IPv4Address::setPort(uint32_t port) {

}

/**
 * IPv6Address的方法实现
 */
IPv6Address::IPv6Address(uint32_t address, uint32_t port) {

}

const sockaddr *IPv6Address::getAddr() const {

}

socklen_t IPv6Address::getAddrLen() const {

}

std::ostream &IPv6Address::print(std::ostream &os) const {

}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {

}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {

}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {

}

uint32_t IPv6Address::getPort() const {

}

void IPv6Address::setPort(uint32_t port) {

}

/**
 * UnixAddress
 */
UnixAddress::UnixAddress(const std::string &path) {

}

const sockaddr *UnixAddress::getAddr() const {

}

socklen_t UnixAddress::getAddrLen() const {

}

std::ostream &UnixAddress::print(std::ostream &os) const {

}

/**
 * UnknownAddress
 */
UnknownAddress::UnknownAddress() {

}

const sockaddr *UnknownAddress::getAddr() const {

}

socklen_t UnknownAddress::getAddrLen() const {

}

std::ostream &UnknownAddress::print(std::ostream &os) const {

}

}