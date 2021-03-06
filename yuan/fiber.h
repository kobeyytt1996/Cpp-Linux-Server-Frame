#ifndef __YUAN_FIBER_H__
#define __YUAN_FIBER_H__
/**
 * @brief 封装协程所需的API。协程是用户级线程：https://zhuanlan.zhihu.com/p/172471249
 * 协程使用上是各个调用栈之间的切换。一个函数执行到某个位置可以切换到另一个函数的某一位置。
 * 设计思路：不是随意可调用的协程，而是 Thread->main_fiber->sub_fiber，main_fiber（每个线程的第一个协程）来创建子协程，并控制所有的sub_fiber是否执行。
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
    // function是C++11最好用的特性之一。解决了函数指针参数个数不灵活的问题，统一了所有functor类型。
    // use_caller同Scheduler构建时的参数含义。为true时，该协程该协程不是线程的主协程，但是Scheduler在此线程的主协程
    Fiber(std::function<void()> cb, size_t stackSize = 0, bool use_caller = false);
    ~Fiber();

    // 可能任务已经执行完，但可以重复利用已分配好的内存,再执行其他任务
    // 只有在INIT、Term和EXCEPT的协程可以调用该方法
    void reset(std::function<void()> cb);
    // 由Scheduler的主协程（执行Scheduler的run方法）切换到当前协程执行。和call作区分。
    void swapIn();
    // 让出执行权（切换到后台）,让Scheduler的主协程运行。和back区分
    void swapOut();
    // 从线程的主协程切换到本协程执行
    void call();
    // 切回到线程的主协程
    void back();

    uint64_t getId() const { return m_id; }
    State getState() const { return m_state; }
    void setState(State state) { m_state = state; }
public:
    // 设置当前协程
    static void SetThis(Fiber *fiber);
    // 获取当前在运行的协程。如果没有主协程，会创建一个
    static Fiber::ptr GetThis();
    static uint64_t GetFiberId();
    // 改变当前协程的状态。协程切换到后台，执行权还给Scheduler的主协程，
    // 并且设置相应状态。在run方法里会区分，Hold为等某些条件被触发后再加入到任务队列
    static void YieldToHold();
    // 切换到Ready则应立马再次被加到任务队列
    static void YieldToReady();

    // 用来统计一共用了多少个协程
    static uint64_t TotalFibers();
    // 切换协程（context）时开始执行的函数。只有线程的主协程不执行此方法。
    static void MainFunc();
    // 与MainFunc基本一样。唯一区别是用swapOut还是back
    static void CallerMainFunc();
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