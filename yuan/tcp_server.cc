#include "config.h"
#include "log.h"
#include "tcp_server.h"

namespace yuan {

static Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

static yuan::ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout = 
    yuan::Config::Lookup("tcp_server.read_timeout", (uint64_t)60 * 1000 * 2, "tcp_server read_timeout");

TcpServer::TcpServer(IOManager *worker, IOManager *accept_worker)
    : m_worker(worker)
    , m_acceptWorker(accept_worker)
    , m_recvTimeout(g_tcp_server_read_timeout->getValue())
// 命名格式为：功能/版本号。比如框架进行了升级，那么可以用版本号进行标识
    , m_name("yuan/1.0.0")
    , m_isStop(true) {}

TcpServer::~TcpServer() {
    for (auto &sock : m_socks) {
        sock->close();
    }
}

bool TcpServer::bind(yuan::Address::ptr addr) {
    std::vector<yuan::Address::ptr> addrs(1, addr);
    std::vector<yuan::Address::ptr> fails;
    return bind(addrs, fails);
}

bool TcpServer::bind(const std::vector<yuan::Address::ptr> &addrs, std::vector<Address::ptr> &fails) {
    for (auto &addr : addrs) {
        Socket::ptr sock = Socket::CreateTCP(addr);
        if (!sock->bind(addr)) {
            YUAN_LOG_ERROR(g_system_logger) << "bind fail errno=" << errno << " strerr="
                << strerror(errno) << " addr=[" << addr->toString() << "]";
                fails.push_back(addr);
                continue;
        }
        if (!sock->listen()) {
            YUAN_LOG_ERROR(g_system_logger) << "listen fail errno=" << errno << " strerr="
                << strerror(errno) << " addr=[" << addr->toString() << "]";
                fails.push_back(addr);
                continue;
        }
        m_socks.push_back(sock);
    }

    if (!fails.empty()) {
        m_socks.clear();
        return false;
    }

    for (auto &sock : m_socks) {
        YUAN_LOG_INFO(g_system_logger) << "server bind success: " << *sock;
    }
    return true;
}

bool TcpServer::start() {
    if (!m_isStop) {
        return true;
    }
    m_isStop = false;
    for (auto &sock : m_socks) {
        m_acceptWorker->schedule(std::bind(&TcpServer::startAccept, shared_from_this(), sock));
    }
    return true;
}

void TcpServer::stop() {
    m_isStop = true;
    // 细节：还是为了保证对象仍然存在
    auto self = shared_from_this();
    m_acceptWorker->schedule([self]{
        for (auto &sock : self->m_socks) {
            // 细节：一定要cancel，防止仍有协程在等着accept
            sock->cancelAll();
            sock->close();
        }
        self->m_socks.clear();
    });
}

void TcpServer::handleClient(Socket::ptr client) {
    YUAN_LOG_INFO(g_system_logger) << "handle client: " << *client;
    // 注意这里client被自动销毁，所指socket会调用析构函数，也就是close，会断开连接。
}

void TcpServer::startAccept(Socket::ptr sock) {
    while (!m_isStop) {
        Socket::ptr client = sock->accept();
        if (client) {
            client->setRecvTimeout(m_recvTimeout);
            // 重点：注意这里shared_from_this的使用，确保tcp_server的生命周期，不会提前被释放掉
            m_worker->schedule(std::bind(&TcpServer::handleClient, shared_from_this(), client));
        } else {
            YUAN_LOG_ERROR(g_system_logger) << "accept errno=" << errno << "strerr"
                << strerror(errno);
        }
    }
}

}