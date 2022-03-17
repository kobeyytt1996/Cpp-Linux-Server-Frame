#include "../yuan/yuan_all_headers.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

int main() {
    yuan::Scheduler scheduler;
    scheduler.start();
    scheduler.stop();

    return 0;
}