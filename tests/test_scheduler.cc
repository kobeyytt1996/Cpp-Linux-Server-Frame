#include "../yuan/yuan_all_headers.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

int main() {
    YUAN_LOG_INFO(g_logger) << "main start";
    yuan::Scheduler scheduler;
    scheduler.start();
    scheduler.stop();
    YUAN_LOG_INFO(g_logger) << "main end";

    return 0;
}