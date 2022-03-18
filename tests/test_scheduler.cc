#include "../yuan/yuan_all_headers.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void test_fiber() {
    static int s_count = 5;
    YUAN_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    sleep(1);
    if (--s_count >= 0) {
        // 指定每次执行的线程都和最先执行的线程相同
        yuan::Scheduler::GetThis()->schedule(&test_fiber, yuan::GetThreadId());
    }
}

int main() {
    YUAN_LOG_INFO(g_logger) << "main start";
    // 这样使用，则当前线程既存在自己的主协程，在运行下方代码，也存在Scheduler的主协程，在运行run。start后控制权交给Scheduler的主协程
    yuan::Scheduler scheduler(3, false, "test");
    scheduler.start();
    sleep(2);
    // 该任务可以在任意线程里的子协程中运行
    scheduler.schedule(&test_fiber);
    scheduler.stop();
    YUAN_LOG_INFO(g_logger) << "main over";

    return 0;
}