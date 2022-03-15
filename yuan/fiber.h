#ifndef __YUAN_FIBER_H__
#define __YUAN_FIBER_H__
/**
 * @brief 封装协程所需的API
 * 不是随意可调用的协程，而是 Thread->main_fiber->sub_fiber，main_fiber来创建子协程，并控制所有的sub_fiber是否执行。
 * 牺牲灵活性，但更好控制
 */
// 协程相关的库
#include <ucontext.h>
#include <functional>
#include <memory>
#include "thread.h"

namespace yuan {

class Fiber : std::enable_shared_from_this<Fiber> {
public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State {
        INIT,
        HOLD,
        EXEC,
        TERM,
        READY,
        // 相当于TERM，只是异常退出
        EXCEPT
    };

private:
    Fiber();

public:
    // function是C++11最好用的特性之一。解决了函数指针参数个数不灵活的问题，统一了所有functor类型
    Fiber(std::function<void()> cb, size_t stackSize = 0);
    ~Fiber();

    // 可能任务已经执行完，但可以重复利用已分配好的内存,再执行其他任务
    // 只有在INIT和Term的协程可以调用该方法
    void reset(std::function<void()> cb);
    // 由主协程切换到当前协程执行
    void swapIn();
    // 让出执行权（切换到后台）,让主协程运行
    void swapOut();

    uint64_t getId() const { return m_id; }
public:
    // 设置当前协程
    static void SetThis(Fiber *fiber);
    // 获取当前在运行的协程
    static Fiber::ptr GetThis();
    static uint64_t GetFiberId();
    // 改变当前协程的状态。协程切换到后台，并且设置相应状态
    static void YieldToHold();
    static void YieldToReady();

    // 用来统计一共用了多少个协程
    static uint64_t TotalFibers();
    // 切换协程（context）时开始执行的函数
    static void MainFunc();
private:
    uint64_t m_id = 0;
    uint32_t m_stacksize = 0;
    void *m_stack = nullptr;
    State m_state = INIT;
    // 使用该头文件提供的协程控制API
    ucontext_t m_ctx;
    // void()因为协程库API传入的工作函数也是该signature
    std::function<void()> m_cb;
};

}


#endif