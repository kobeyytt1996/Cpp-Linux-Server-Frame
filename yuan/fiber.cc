#include "fiber.h"
#include <atomic>
#include "config.h"
#include "macro.h"

namespace yuan {

// 以下两个为对协程的统计量
static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

// 用来记录主线程中的主协程
static thread_local Fiber *t_fiber = nullptr;
static thread_local std::shared_ptr<Fiber::ptr> t_threadFiber = nullptr;

// 先约定协程的栈大小为1MB，之后可以通过配置文件修改
static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
    Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");

// 自定义一个内存分配器，可能有多种实现，比如malloc，mmap等。看了侯捷老师的内存课后，这里可以优化
class MallocStackAllocator {
public:
    static void *Alloc(size_t size) {
        return malloc(size);
    }

    // 这里传size的原因是考虑到以后可能调用munmap，所以兼容
    static void Dealloc(void *vp, size_t size) {
        free(vp);
    }
};

// 增加灵活性，想改实现把等号右边改一下即可
using StackAllocator = MallocStackAllocator;

// 私有构造方法，只有主协程会使用这个方法
Fiber::Fiber() {
    m_state = EXEC;
    SetThis(this);

    // 协程相关，即获取上下文，可以man来了解
    if (getcontext(&m_ctx)) {
        YUAN_ASSERT2(false, "getContext");
    }

    ++s_fiber_count;
}

// 这个方法是真正构造工作的协程
Fiber::Fiber(std::function<void()> cb, size_t stackSize = 0) : m_id(++s_fiber_id), m_cb(cb) {
    ++s_fiber_count;
    // 除了一些特殊任务需要大的栈空间，其他都用全局约定好的
    m_stacksize = stackSize ? stackSize : g_fiber_stack_size->getValue();

    m_stack = StackAllocator::Alloc(m_stacksize);
    if (getcontext(&m_ctx)) {
        YUAN_ASSERT2(false, "getContext");
    }

    // 看man makecontext，切换上下文前的准备
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;
    
    makecontext(&m_ctx, MainFunc, 0);
}

Fiber::~Fiber() {
    --s_fiber_count;
    // 根据是否是主协程做不同的操作。主协程没有栈
    if (m_stack) {
        YUAN_ASSERT(m_state == TERM || m_state == INIT);
        StackAllocator::Dealloc(&m_stack, m_stacksize);
    } else {
        YUAN_ASSERT(!m_cb);
        YUAN_ASSERT(m_state == EXEC);

        if (t_fiber == this) {
            SetThis(nullptr);
        }
    }

}

void Fiber::reset(std::function<void()> cb) {

}

void Fiber::swapIn() {

}

void Fiber::swapOut() {

}

void Fiber::SetThis(Fiber *fiber) {

}


Fiber::ptr GetThis() {

}
    
void YieldHold() {

}

void YieldReady() {

}

uint64_t TotalFibers() {

}

void MainFunc() {

}
}