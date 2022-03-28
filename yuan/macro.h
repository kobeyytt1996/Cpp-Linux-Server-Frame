#ifndef __YUAN_MACRO_H__
#define __YUAN_MACRO_H__
/**
 * @brief 从协程模块开始，定义一些便于后续调试的宏
 * 
 */

#include <string.h>
#include <assert.h>
#include "util.h"
#include "log.h"

// 以下先判断编译器，是这两个编译器，则进行指令优化：https://www.jianshu.com/p/2684613a300f
#if defined __GNUC__ || defined __llvm__
#define YUAN_LIKELY(x)    __builtin_expect(!!(x), 1)
#define YUAN_UNLIKELY(x)    __builtin_expect(!!(x), 0)
#else 
#define YUAN_LIKELY(x)    (x)
#define YUAN_UNLIKELY(x)    (x)
#endif

// assert报错后只能返回assert有问题的那一行信息。而获取到整个函数调用栈更好debug一些。可以man backtrace了解
// ASSERT的事情都较少可能发生，所以用YUAN_UNLIKELY
#define YUAN_ASSERT(x) \
    if (YUAN_UNLIKELY(!(x))) { \
        YUAN_LOG_ERROR(YUAN_GET_ROOT_LOGGER()) << "ASSERTION: " #x \
            << "\nbacktrace\n" << yuan::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#define YUAN_ASSERT2(x, w) \
    if (YUAN_UNLIKELY(!(x))) { \
        YUAN_LOG_ERROR(YUAN_GET_ROOT_LOGGER()) << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace\n" << yuan::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif