#ifndef __YUAN_FIBER_H__
#define __YUAN_FIBER_H__
/**
 * @brief 封装协程所需的API
 * 不是随意可调用的协程，而是 Thread->main_fiber->sub_fiber，main_fiber（每个线程的第一个协程）来创建子协程，并控制所有的sub_fiber是否执行。
 * 即子协程之间不能切换，只能和主协程切换
 * 牺牲灵活性，但更好控制
 */
// 协程相关的库
#include <ucontext.h>
#include <functional>
#include <memory>
#include "thread.h"

namespace yuan {

// 注意必须public继承，否则会报错bad_weak_ptr，原因尚不明。猜测是用shared_ptr初始化时，无法使用到enable_shared_from_this的特性
class Fiber : public std::enable_shared_from_this<Fiber> {
public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State {
        INIT,
        HOLD,
        READY,
        EXEC,
        TERM,
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
    // 只有在INIT、Term和EXCEPT的协程可以调用该方法
    void reset(std::function<void()> cb);
    // 由Scheduler的主协程（执行Scheduler的run方法）切换到当前协程执行
    void swapIn();
    // 让出执行权（切换到后台）,让Scheduler的主协程运行
    void swapOut();
    // 从线程的主协程切换到本协程执行
    void call();

    uint64_t getId() const { return m_id; }
    State getState() const { return m_state; }
    void setState(State state) { m_state = state; }
public:
    // 设置当前协程
    static void SetThis(Fiber *fiber);
    // 获取当前在运行的协程。如果没有主协程，会创建一个
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
    // 主协程构造函数里不设置m_id,都为0
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