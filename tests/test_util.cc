#include "../yuan/yuan_all_headers.h"
#include <assert.h>

yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void test_assert() {
    YUAN_LOG_INFO(g_logger) << yuan::BacktraceToString(10);
}

int main() {
    test_assert();
    return 0;
}