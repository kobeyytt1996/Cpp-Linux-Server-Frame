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

// assert报错后只能返回assert有问题的那一行信息。而获取到整个函数调用栈更好debug一些。可以man backtrace了解
#define YUAN_ASSERT(x) \
    if (!(x)) { \
        YUAN_LOG_ERROR(YUAN_GET_ROOT_LOGGER()) << "ASSERTION: " #x \
            << "\nbacktrace\n" << yuan::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#define YUAN_ASSERT2(x, w) \
    if (!(x)) { \
        YUAN_LOG_ERROR(YUAN_GET_ROOT_LOGGER()) << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace\n" << yuan::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif