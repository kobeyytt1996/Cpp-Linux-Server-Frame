#include "thread.h"
#include "log.h"
#include "util.h"

namespace yuan {

// thread_local非常有用，改变了生命周期（存储周期）：https://murphypei.github.io/blog/2020/02/thread-local
// 为了获取当前程序所在线程，指向当前线程
static thread_local Thread *t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOWN";

// 系统的库打日志的时候统一用叫system的logger。与业务区分
static Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

/**
 * @brief 以下是信号量相关的定义
 */
Semaphore::Semaphore(uint32_t count) {
    if (sem_init(&m_semaphore, 0, count)) {
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore() {
    sem_destroy(&m_semaphore);
}

void Semaphore::wait() {
    if (sem_wait(&m_semaphore)) {
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::post() {
    if (sem_post(&m_semaphore)) {
        throw std::logic_error("sem_post error");
    }
}


/**
 * @brief 以下是线程相关的定义
 */
Thread *Thread::GetThis() {
    return t_thread;
}

const std::string Thread::GetName() {
    return t_thread_name;
}

void Thread::SetName(const std::string &name) {
    // 是我们自己创建的线程
    if (t_thread) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

// 注意构造函数是在主线程执行
Thread::Thread(std::function<void()> cb, const std::string &name) : m_cb(cb), m_name(name) {
    if (name.empty()) {
        m_name = "UNKNOWN";
    }
    int ret = pthread_create(&m_thread, nullptr, run, this);
    if (ret) {
        YUAN_LOG_ERROR(g_system_logger) << "pthread_create fail, ret: " << ret
            << " name= " << name;
        throw std::logic_error("pthread_create error");
    }
    // 可以让主线程创建Thread后，等run里初始化后才创建下一个Thread，或其他事。确保线程已经运行
    m_semaphore.wait();
}

Thread::~Thread() {
    if (m_thread) {
        // 可以选择join，但会阻塞。选detach的隐患是之后GetThis获取到的是野指针
        pthread_detach(m_thread);
    }
}


void Thread::join() {
    if (m_thread) {
        int ret = pthread_join(m_thread, nullptr);
        if (ret) {
            YUAN_LOG_ERROR(g_system_logger) << "pthread_join fail, ret: " << ret
                << " name= " << m_name;
            throw std::logic_error("pthread_join error");
        }
        // 注意细节
        m_thread = 0;
    }
}

void *Thread::run(void *arg) {
    Thread *thread = static_cast<Thread*>(arg);
    t_thread = thread;
    t_thread_name = thread->getName();
    thread->m_id = GetThreadId();
    // 修改名字后，shell里top就能看到线程相应的名字，但这个名字最多16字符
    // shell里: ps aux | grep 程序名 得到进程号后：top -H -p 进程号  可以看到所有线程
    pthread_setname_np(pthread_self(), thread->getName().substr(0, 15).c_str());

    std::function<void()> cb;
    // 有效减少m_cb中可能存在的智能指针的引用
    cb.swap(thread->m_cb);

    thread->m_semaphore.post();
    cb();
    return 0;
}


}