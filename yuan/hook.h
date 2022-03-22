#ifndef __YUAN_HOOK_H__
#define __YUAN_HOOK_H__

#include <unistd.h>

namespace yuan {
    bool is_hook_enable();
    // 可以决定哪些地方需要hook，哪些线程需要hook。会实现到线程级hook，线程内被hook的都会变为自己的实现
    void set_hook_enable(bool flag);
}

// C的编译规则里没有重载。而C++有，即编译器会通过不同的函数签名生成不同的函数名字来区分重载。
// 但这里我们指明使用C的编译规则，不使用重载。叫什么名字编译器就输出什么名字
extern "C" {

typedef unsigned int (*sleep_fun)(unsigned int seconds);
// 用来保存原来的sleep实现，这样随时可以和新的切换
extern sleep_fun sleep_f;

typedef int (*usleep_fun)(useconds_t usec);
extern usleep_fun usleep_f;

}

#endif