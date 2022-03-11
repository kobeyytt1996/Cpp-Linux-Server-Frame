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

// 下面两个函数用来测试不加锁的日志系统是否有线程安全问题
void fun2() {
    for (int i = 0; i < 20000; ++i) {
        YUAN_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    }
}

void fun3() {
    for (int i = 0; i < 20000; ++i) {
        YUAN_LOG_INFO(g_logger) << "========================================";
    }
}

int main() {
    YUAN_LOG_INFO(g_logger) << "test thread begin";
    YAML::Node root = YAML::LoadFile("/home/yuan/workspace/yuan/bin/conf/log2.yml");
    yuan::Config::LoadFromYaml(root);
    std::vector<yuan::Thread::ptr> thrs;
    for (int i = 0; i < 10; ++i) {
        yuan::Thread::ptr thread(new yuan::Thread(fun2, "name_" + std::to_string(i * 2)));
        yuan::Thread::ptr thread1(new yuan::Thread(fun3, "name_" + std::to_string(i * 2 + 1)));
        thrs.push_back(thread);
        thrs.push_back(thread1);
    }

    for (auto &ptr : thrs) {
        ptr->join();
    }
    YUAN_LOG_INFO(g_logger) << "test thread end";
    YUAN_LOG_INFO(g_logger) << "count: " << g_count;


    return 0;
}