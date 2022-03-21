#include "./yuan/iomanager.h"
#include "../yuan/yuan_all_headers.h"

#include <arpa/inet.h>
#include <sys/types.h>    
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void test_fiber() {
    YUAN_LOG_INFO(g_logger) << "test_fiber";

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock_fd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    // 先用百度的IP地址测试
    inet_pton(AF_INET, "39.156.66.14", &addr.sin_addr.s_addr);
    addr.sin_port = htons(80);

    // yuan::IOManager::GetThis()->addEvent(sock_fd, yuan::IOManager::READ, [sock_fd](){
    //     YUAN_LOG_INFO(g_logger) << "read callback";
    // });
    yuan::IOManager::GetThis()->addEvent(sock_fd, yuan::IOManager::WRITE, [sock_fd](){
        YUAN_LOG_INFO(g_logger) << "connected to Baidu";
        // 主动close后，上面监听的读事件不会在epoll_wait里触发，需要主动cancel掉
        // yuan::IOManager::GetThis()->cancelEvent(sock_fd, yuan::IOManager::READ);
        close(sock_fd);
    });
    
    // 会唤醒idle里的epoll_wait,并将上述回调事件放入消息队列中，由Scheduler主协程调度工作协程执行
    int ret = connect(sock_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret) {
        YUAN_LOG_ERROR(g_logger) << "epoll_ctl:" <<  ret << " (" << errno << ") (" << strerror(errno) << ")";
        return;
    }
}

void test1() {
    yuan::IOManager iomanager(2, false);
    iomanager.schedule(&test_fiber);
}

yuan::Timer::ptr s_timer;
// 测试带有TimerManager实现的iomanager
void test_timer() {
    yuan::IOManager iomanager(2);
    s_timer = iomanager.addTimer(1000, [](){
        static int i = 0;
        YUAN_LOG_INFO(g_logger) << "hello timer i = " << i;
        if (++i == 3) {
            s_timer->reset(2000, true);
            // s_timer->cancel();
        }
    }, true);
}

int main() {
    // test1();
    test_timer();
    return 0;
}