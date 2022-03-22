#include "fiber.h"
#include "hook.h"
#include "iomanager.h"

// 编译的时候要加上链接库：dl
#include <dlfcn.h>
#include <functional>

namespace yuan {

// hook是以线程为单位，故使用thread_local
static thread_local bool t_hook_enable = false;

// 要hook的函数列表
#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep)

// 将我们声明的变量指针指向要hook的系统的原函数
void hook_init() {
    static bool is_inited = false;
    if (is_inited) {
        return;
    }
// 重点：##表示字符串连接。dlsym:https://blog.csdn.net/mijichui2153/article/details/109561978。或看man
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX)
#undef XX
}

// 要在main函数之前初始化hook。利用数据初始化的顺序，如全局变量在main进入之前就初始化。把要做的放在构造方法里。
struct _HookIniter {
    _HookIniter() {
        hook_init();
    }
};

static _HookIniter s_hook_initer;

bool is_hook_enable() {
    return t_hook_enable;
}

void set_hook_enable(bool flag) {
    t_hook_enable = flag;
}

}

extern "C" {

// 初始化要hook的函数指针
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX)
#undef XX

// hook sleep的实现。原系统的sleep会让整个线程sleep，这里只是让fiber sleep
unsigned int sleep(unsigned int seconds) {
    if (!yuan::t_hook_enable) {
        // 不hook则使用系统的原函数
        return sleep_f(seconds);
    }

    yuan::Fiber::ptr fiber = yuan::Fiber::GetThis();
    yuan::IOManager *iomanager = yuan::IOManager::GetThis();
    // sleep被hook为，当前协程放弃执行权，添加定时器，到了指定时间再被放到任务队列里被执行
    // iomanager->addTimer(seconds * 1000, std::bind(&yuan::Scheduler::schedule<yuan::Fiber::ptr>, *iomanager, fiber));
    iomanager->addTimer(seconds * 1000, [iomanager, fiber](){
        iomanager->schedule(fiber);
    });
    yuan::Fiber::YieldToHold();

    return 0;
}

// 与sleep的实现一致，这里的时间单位是微妙
int usleep(useconds_t usec) {
    if (!yuan::t_hook_enable) {
        // 不hook则使用系统的原函数
        return usleep_f(usec);
    }

    yuan::Fiber::ptr fiber = yuan::Fiber::GetThis();
    yuan::IOManager *iomanager = yuan::IOManager::GetThis();
    // sleep被hook为，当前协程放弃执行权，添加定时器，到了指定时间再被放到任务队列里被执行
    // iomanager->addTimer(usec / 1000, std::bind(&yuan::Scheduler::schedule<yuan::Fiber::ptr>, *iomanager, fiber));
    iomanager->addTimer(usec / 1000, [iomanager, fiber](){
        iomanager->schedule(fiber);
    });
    yuan::Fiber::YieldToHold();

    return 0;
}

}


extern sleep_fun sleep_f;

extern usleep_fun usleep_f;