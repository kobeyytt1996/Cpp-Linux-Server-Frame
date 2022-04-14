#include "../yuan/http/http_server.h"
#include "../yuan/log.h"

/**
 * 测试我们http服务器性能的程序。
 */

yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void run() {
    // 设定日志级别为INFO。实际运行中，DEBUG日志很少打出来
    g_logger->setLevel(yuan::LogLevel::INFO);
    yuan::Address::ptr addr = yuan::Address::LookupAnyIPAdress("0.0.0.0:8020");
    if (!addr) {
        YUAN_LOG_ERROR(g_logger) << "get address fail";
        return;
    }

    yuan::http::HttpServer::ptr server(new yuan::http::HttpServer(true));
    while (!server->bind(addr)) {
        YUAN_LOG_ERROR(g_logger) << "bind or listen fail:" << *addr;
        sleep(1);
    }
    server->start();
}

int main (int argc, char **argv) {
    yuan::IOManager iom(4);
    iom.schedule(run);

    return 0;
}
