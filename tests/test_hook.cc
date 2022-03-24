#include "../yuan/hook.h"
#include "../yuan/log.h"
#include "../yuan/iomanager.h"

#include <arpa/inet.h>
#include <sys/types.h>    
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void test_sleep() {
    yuan::IOManager iomanager(1);
    // 如果sleep成功，该线程总共只睡3s，否则5s
    iomanager.schedule([](){
        sleep(2);
        YUAN_LOG_INFO(g_logger) << "sleep 2";
    });
    iomanager.schedule([](){
        sleep(3);
        YUAN_LOG_INFO(g_logger) << "sleep 3";
    });
    YUAN_LOG_INFO(g_logger) << "test_sleep";
}

void test_sock_cb() {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    // 先用百度的IP地址测试
    inet_pton(AF_INET, "110.242.68.4", &addr.sin_addr.s_addr);
    addr.sin_port = htons(80);

    YUAN_LOG_INFO(g_logger) << "begin connect";
    int ret = connect(sock_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    YUAN_LOG_INFO(g_logger) << "connect ret = " << ret << " errno = " << errno;

    if (ret) {
        return;
    }

    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    ret = send(sock_fd, data, sizeof(data), 0);
    YUAN_LOG_INFO(g_logger) << "send ret = " << ret << " errno = " << errno;

    if (ret <= 0) {
        return;
    }

    // 协程的栈都分配的很小，所以不用const char*的形式
    std::string buff;
    buff.resize(4096);
    // 细节：这样用string也可以利用系统函数
    ret = recv(sock_fd, &buff[0], buff.size(), 0);
    YUAN_LOG_INFO(g_logger) << "recv ret = " << ret << " errno = " << errno;

    if (ret <= 0) {
        return;
    }
    buff.resize(ret);
    YUAN_LOG_INFO(g_logger) << buff;
}

void test_sock_io() {
    yuan::IOManager iomanager(1, false);
    iomanager.schedule(test_sock_cb);
}

int main() {
    // test_sleep();
    test_sock_io();
    return 0;
}