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

namespace yuan {

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
};

}

#endif