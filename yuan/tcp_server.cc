#include "config.h"
#include "log.h"
#include "tcp_server.h"

namespace yuan {

static Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

static yuan::ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout = 
    yuan::Config::Lookup("tcp_server.read_timeout", (uint64_t)60 * 1000 * 2, "tcp_server read_timeout");

TcpServer::TcpServer(IOManager *worker)
    : m_worker(worker)
    , m_readTimeout(g_tcp_server_read_timeout->getValue())
// 命名格式为：功能/版本号。比如框架进行了升级，那么可以用版本号进行标识
    , m_name("yuan/1.0.0")
    , m_isStop(false) {}

bool TcpServer::bind(yuan::Address::ptr addr) {
    std::vector<yuan::Address::ptr> addrs(1, addr);
    return bind(addrs);
}

bool TcpServer::bind(const std::vector<yuan::Address::ptr> &addrs) {
    // 传入的多个地址可能只有部分可以bind。比如有一些端口号已被占用
    bool ret = true;
}

bool TcpServer::start() {

}

bool TcpServer::stop() {

}

}