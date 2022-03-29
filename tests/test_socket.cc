#include "../yuan/iomanager.h"
#include "../yuan/socket.h"
#include "../yuan/yuan_all_headers.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void test_socket() {
    yuan::IPAddress::ptr addr = yuan::Address::LookupAnyIPAdress("www.baidu.com");
    if (!addr) {
        YUAN_LOG_ERROR(g_logger) << "get address fail";
        return;
    } else {
        YUAN_LOG_INFO(g_logger) << "get address: " << addr->toString();
    }
    addr->setPort(80);

    yuan::Socket::ptr sock = yuan::Socket::CreateTCP(addr);
    if (!sock->connect(addr)) {
        YUAN_LOG_ERROR(g_logger) << "connect " << addr->toString() << " fail";
        return;
    } else {
        YUAN_LOG_INFO(g_logger) << "connect " << addr->toString() << " connected";
    }

    const char buff[] = "GET / HTTP/1.0\r\n\r\n";
    int ret = sock->send(buff, sizeof(buff), 0);
    if (ret < 0) {
        YUAN_LOG_ERROR(g_logger) << "send fail ret=" << ret;
        return;
    }

    std::string recv_buff;
    recv_buff.resize(4096);
    ret = sock->recv(&recv_buff[0], recv_buff.size(), 0);
    if (ret < 0) {
        YUAN_LOG_ERROR(g_logger) << "recv fail ret=" << ret;
        return;
    }

    recv_buff.resize(ret);
    YUAN_LOG_INFO(g_logger) << recv_buff;
}

int main(int argc, char **argv) {
    yuan::IOManager iom;
    iom.schedule(test_socket);
    return 0;
}