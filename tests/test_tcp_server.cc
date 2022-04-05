#include "../yuan/tcp_server.h"
#include "../yuan/iomanager.h"
#include "../yuan/log.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void run() {
    auto addr = yuan::Address::LookupAny("0.0.0.0:8034");
    auto addr2 = std::make_shared<yuan::UnixAddress>("/tmp/unix_addr");
    YUAN_LOG_INFO(g_logger) << *addr << " - " << *addr2;

    std::vector<yuan::Address::ptr> addrs{addr, addr2}, fails;
    yuan::TcpServer::ptr tcp_server = std::make_shared<yuan::TcpServer>();
    while (!tcp_server->bind(addrs, fails)) {
        sleep(2);
    }
    tcp_server->start();
}

int main() {
    yuan::IOManager iom(2);
    iom.schedule(run);
    return 0;
}