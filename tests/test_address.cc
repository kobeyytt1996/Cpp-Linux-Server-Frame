#include <vector>

#include "../yuan/address.h"
#include "../yuan/log.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

// 测试根据域名获取IP地址
void test() {
    std::vector<yuan::Address::ptr> addr_vec;
    bool ret = yuan::Address::Lookup(addr_vec, "www.sylar.top:http");
    if (!ret) {
        YUAN_LOG_ERROR(g_logger) << "lookup fail";
        return;
    }

    for (size_t i = 0; i < addr_vec.size(); ++i) {
        YUAN_LOG_INFO(g_logger) << addr_vec[i]->toString();
    }
}

// 测试获取网卡地址
void test_iface() {
    std::multimap<std::string, std::pair<yuan::Address::ptr, uint32_t>> iface_map;
    bool ret = yuan::Address::GetInterfaceAddresses(iface_map);

    if (!ret) {
        YUAN_LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
        return;
    }

    for (auto &i : iface_map) {
        YUAN_LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - "
            << i.second.second;
    }

}

void test_ipv4() {
    // auto addr = yuan::IPAddress::Create("www.sylar.top");
    auto addr = yuan::IPAddress::Create("127.0.0.8");
    if (addr) {
        YUAN_LOG_INFO(g_logger) << addr->toString();
    }
}

int main(int argc, char **argv) {
    test();
    // test_iface();
    // test_ipv4();
    return 0;
}