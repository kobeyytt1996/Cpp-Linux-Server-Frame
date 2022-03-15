#include "../yuan/yuan_all_headers.h"

yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void run_in_fiber() {
    YUAN_LOG_INFO(g_logger) << "run_in_fiber begin";
    // 交换给主协程
    yuan::Fiber::YieldToHold();
    YUAN_LOG_INFO(g_logger) << "run_in_fiber end";
    yuan::Fiber::YieldToHold();
}

void test_fiber() {
    {
        // 注意当前实现要先调用该方法初始化main协程
        yuan::Fiber::GetThis();
        YUAN_LOG_INFO(g_logger) << "main begin";
        yuan::Fiber::ptr fiber(new yuan::Fiber(run_in_fiber));
        // 只能在主协程上进行切换
        fiber->swapIn();
        YUAN_LOG_INFO(g_logger) << "main after swapin";
        fiber->swapIn();
        YUAN_LOG_INFO(g_logger) << "main after end";
        fiber->swapIn();
    }
    YUAN_LOG_INFO(g_logger) << "main after end2";
}

int main() {
    // 打日志时便于区分线程
    yuan::Thread::SetName("main");
    
    // 多线程多协程使用示范
    std::vector<yuan::Thread::ptr> thrs;
    for (int i = 0; i < 3; ++i) {
        thrs.emplace_back(new yuan::Thread(test_fiber, "thread_" + std::to_string(i)));
    }
    for (auto &th : thrs) {
        th->join();
    }
    return 0;
}