/**
 * @file thread.h
 * @brief 封装的线程类，因为之后主要用协程，因此这里封装简单的线程相关的即可，主要作为协程的容器
 * 常用pthread线程库，c++11后增加了std::thread（内部也是调用前者）。但是std::thread没有读写锁。
 * 因为业务大概率是读多写少，因此还是使用pthread线程库
 * @version 0.1
 * @date 2022-03-09
 * 
 */

#ifndef __YUAN_THREAD_H__
#define __YUAN_THREAD_H__

#include <thread>
#include <functional>
#include <memory> 
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

namespace yuan {

// 在服务器中常用来管理消息队列，有生产消息和消费消息的
class Semaphore {
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();

    void wait();
    void post();
private:
    // 不允许拷贝和move
    Semaphore(const Semaphore &) = delete;
    Semaphore(const Semaphore &&) = delete;
    Semaphore &operator=(const Semaphore &) = delete;
    Semaphore &operator=(const Semaphore &&) = delete;

private:
    sem_t m_semaphore;
};

/**
 * @brief 锁的封装类，方便使用，通常用作局部变量，即便有异常，析构函数也能解锁。
 * 因为锁的种类很多，但可以封装出相同的接口。所以做成模板类，T为具体的锁
 */
template<typename T>
class ScopedMutexImpl {
public:
    ScopedMutexImpl(T &mutex) : m_mutex(mutex), m_locked(false) {
        lock();
    }

    ~ScopedMutexImpl() {
        // 析构函数解锁，尽量不出现死锁
        unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T &m_mutex;
    // 防止死锁，判断是否已加锁
    bool m_locked;
};

template<typename T>
class ReadScopedMutexImpl {
public:
    ReadScopedMutexImpl(T &mutex) : m_mutex(mutex), m_locked(false) {
        lock();
    }

    ~ReadScopedMutexImpl() {
        // 析构函数解锁，尽量不出现死锁
        unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T &m_mutex;
    // 防止死锁，判断是否已加锁
    bool m_locked;
};

template<typename T>
class WriteScopedMutexImpl {
public:
    WriteScopedMutexImpl(T &mutex) : m_mutex(mutex), m_locked(false) {
        lock();
    }

    ~WriteScopedMutexImpl() {
        // 析构函数解锁，尽量不出现死锁
        unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    T &m_mutex;
    // 防止死锁，判断是否已加锁
    bool m_locked;
};

// 普通锁。析构函数确保销毁锁。具体怎么用看test_thread.cc
class Mutex {
public:
    typedef ScopedMutexImpl<Mutex> Lock;
    Mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }
private:
    pthread_mutex_t m_mutex;
};

// 读写锁
class RWMutex {
public:
    typedef ReadScopedMutexImpl<RWMutex> ReadLock;
    typedef WriteScopedMutexImpl<RWMutex> WriteLock;
    RWMutex() {
        pthread_rwlock_init(&m_lock, nullptr);
    }

    ~RWMutex() {
        pthread_rwlock_destroy(&m_lock);
    }

    void rdlock() {
        pthread_rwlock_rdlock(&m_lock);
    }

    void wrlock() {
        pthread_rwlock_wrlock(&m_lock);
    }

    void unlock() {
        pthread_rwlock_unlock(&m_lock);
    }
private:
    pthread_rwlock_t m_lock;
};

class Thread {
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()> cb, const std::string &name);
    ~Thread();

    pid_t getId() const { return m_id; }
    const std::string getName() const { return m_name; }

    void join();

    // 供其他函数查询现在正在哪个线程上执行执行
    static Thread *GetThis();  
    // 获取当前线程的名称，日志的时候方便一些
    static const std::string GetName();
    // 有些线程不是我们创建的，如主线程，但也希望它有名字
    static void SetName(const std::string &name);
private:
    // 不允许拷贝和move
    Thread(const Thread &) = delete;
    Thread(const Thread &&) = delete;
    Thread &operator=(const Thread &) = delete;
    Thread &operator=(const Thread &&) = delete;

    // 线程执行的函数
    static void *run(void *arg);
private:
    // 存储内核级线程ID
    pid_t m_id = -1; 
    pthread_t m_thread = 0;
    std::function<void()> m_cb;
    std::string m_name;

    Semaphore m_semaphore;
};

}

#endif