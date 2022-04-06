#include "../yuan/tcp_server.h"
#include "../yuan/log.h"
#include "../yuan/iomanager.h"
#include "../yuan/bytearray.h"

/**
 * example文件夹里放的是像demo一样的程序
 */

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

class EchoServer : public yuan::TcpServer {
public:
    // type用来区分协议是文本格式还是二进制格式
    EchoServer(int type);

    // demo，看看如何实现处理客户端请求的逻辑
    void handleClient(yuan::Socket::ptr client) override;
private:
    // 0为二进制，1为文本
    int m_type = 0;
};

EchoServer::EchoServer(int type) 
    : m_type(type) {}

void EchoServer::handleClient(yuan::Socket::ptr client) {
    YUAN_LOG_INFO(g_logger) << "handleClient " << *client;
    yuan::ByteArray::ptr ba(new yuan::ByteArray());

    while (true) {
        ba->clear();
        std::vector<iovec> iovs;
        ba->getWriteBuffers(iovs, 1024);

        int ret = client->recv(&iovs[0], iovs.size());
        if (ret == 0) {
            YUAN_LOG_INFO(g_logger) << "client close: " << *client;
            break;
        } else if (ret < 0) {
            YUAN_LOG_INFO(g_logger) << "client error ret=" << ret << " errno=" << errno 
                << " errstr=" << strerror(errno);
                break; 
        }
        ba->setPosition(ba->getPosition() + ret);
        // 后面的ba->toString()是读取剩余未读取数据，因此要把position设置到0
        ba->setPosition(0);
        if (m_type == 1) {
            std::cout << ba->toString() << std::flush;
        } else {
            std::cout << ba->toHexString() << std::flush;
        }
    }
}

int type = 1;

void run() {
    EchoServer::ptr es = std::make_shared<EchoServer>(type);
    // 可能从其他主机无法连接该端口号，和防火墙相关，没有开放端口号：https://blog.csdn.net/realjh/article/details/82048492
    auto addr = yuan::Address::LookupAny("0.0.0.0:8020");
    while (!es->bind(addr)) {
        sleep(2);
    }
    es->start();
}

int main(int argc, char **argv) {
    if (argc < 2) {
        YUAN_LOG_WARN(g_logger) << "used as[" << argv[0] << " -t] or [" << argv[0] << " -b]";
        return 0;
    }
    if (strcmp(argv[1], "-b") == 0) {
        type = 0;
    }
    yuan::IOManager iom(2);
    iom.schedule(run);
    return 0;
}