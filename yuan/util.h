#ifndef __YUAN_UTIL_H__
#define __YUAN_UTIL_H__

#include <sys/syscall.h>
#include <sys/types.h>
#include "unistd.h"
#include <stdint.h>


namespace yuan {

// 获取的是内核级线程ID：https://blog.csdn.net/zwtxy1231010/article/details/84951126
pid_t GetThreadId();

uint32_t GetFiberId();
}

#endif