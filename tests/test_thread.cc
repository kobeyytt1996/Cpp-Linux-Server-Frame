#include "../yuan/yuan_all_headers.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();
// 用来测试锁
static int g_count = 0;
// 锁的用法
yuan::RWMutex g_rwmutex;
yuan::Mutex g_mutex;

void fun1() {
    YUAN_LOG_INFO(g_logger) << "name: " << yuan::Thread::GetName()
        << " this.name: " << yuan::Thread::GetThis()->getName()
        << " id: " << yuan::GetThreadId()
        << " this.id: " << yuan::Thread::GetThis()->getId();
    for (int i = 0; i < 100000; ++i) {
        // yuan::RWMutex::WriteLock writeLock(g_rwmutex);
        yuan::Mutex::Lock lock(g_mutex);
        ++g_count;
    }
}


int main() {
    YUAN_LOG_INFO(g_logger) << "test thread begin";
    std::vector<yuan::Thread::ptr> thrs;
    for (int i = 0; i < 5; ++i) {
        yuan::Thread::ptr thread(new yuan::Thread(fun1, "name_" + std::to_string(i)));
        thrs.push_back(thread);
    }

    for (auto &ptr : thrs) {
        ptr->join();
    }
    YUAN_LOG_INFO(g_logger) << "test thread end";
    YUAN_LOG_INFO(g_logger) << "count: " << g_count;

    return 0;
}