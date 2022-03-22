#include "../yuan/hook.h"
#include "../yuan/log.h"
#include "../yuan/iomanager.h"

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

int main() {
    test_sleep();
    return 0;
}