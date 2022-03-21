#include "util.h"
#include <execinfo.h>
#include "log.h"
#include "fiber.h"
#include <sys/time.h>

namespace yuan {

Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

pid_t GetThreadId() {
     return syscall(SYS_gettid);
 }

uint32_t GetFiberId() {
    return Fiber::GetFiberId();
}

void Backtrace(std::vector<std::string> &bt, int size, int skip) {
    // 分配内存尽量在堆上，因为会有多个协程需要切换，栈空间比线程小。所以尽量不在栈上分配大对象
    void **array = static_cast<void**>(malloc(sizeof(void*) * size));
    size_t s = ::backtrace(array, size);

    char **strings = ::backtrace_symbols(array, s);
    if (strings == nullptr) {
        YUAN_LOG_ERROR(g_system_logger) << "backtrace_symbols error";
        return;
    }
    for (size_t i = skip; i < s; ++i) {
        bt.push_back(strings[i]);
    }
    // 这里可能有内存泄漏，因为是二维数组。后续优化
    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string &prefix) {
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for (const auto &str : bt) {
        ss << prefix << str << std::endl;
    }

    return ss.str();
}

uint64_t GetCurrentTimeMS() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000UL + tv.tv_usec / 1000;
}

uint64_t GetCurrentTimeUS() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000UL * 1000 + tv.tv_usec;
}

}