#include "../yuan/yuan_all_headers.h"

yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void run_in_fiber() {
    YUAN_LOG_INFO(g_logger) << "run_in_fiber begin";
    yuan::Fiber::YieldToHold();
    YUAN_LOG_INFO(g_logger) << "run_in_fiber end";
}

int main() {
    // 注意当前实现要先调用该方法初始化main协程
    yuan::Fiber::GetThis();

    YUAN_LOG_INFO(g_logger) << "main begin";
    yuan::Fiber::ptr fiber(new yuan::Fiber(run_in_fiber));
    fiber->swapIn();
    YUAN_LOG_INFO(g_logger) << "main after swapin";
    fiber->swapIn();
    YUAN_LOG_INFO(g_logger) << "main after end";
    return 0;
}