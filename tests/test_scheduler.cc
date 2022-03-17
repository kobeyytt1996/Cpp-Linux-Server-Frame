#include "../yuan/yuan_all_headers.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void test_fiber() {
    YUAN_LOG_INFO(g_logger) << "test in fiber";
}

int main() {
    YUAN_LOG_INFO(g_logger) << "main start";
    // 这样使用，则当前线程既存在自己的主协程，在运行下方代码，也存在Scheduler的主协程，在运行run。start后控制权交给Scheduler的主协程
    yuan::Scheduler scheduler;
    scheduler.start();
    scheduler.schedule(&test_fiber);
    scheduler.stop();
    YUAN_LOG_INFO(g_logger) << "main over";

    return 0;
}