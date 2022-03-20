#ifndef __YUAN_UTIL_H__
#define __YUAN_UTIL_H__

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <vector>
#include <string>

namespace yuan {

// 获取的是内核级线程ID：https://blog.csdn.net/zwtxy1231010/article/details/84951126。全局方法一般大写
pid_t GetThreadId();

uint32_t GetFiberId();

// 很多时候需要调用栈信息，可以man backtrace了解。比如assert报错后只能返回assert有问题的那一行信息。而获取到整个函数调用栈更好debug一些。
// size是要显示多少层栈，skip是忽略前面的多少层
void Backtrace(std::vector<std::string> &bt, int size = 64, int skip = 1);

// skip默认为2，因为自己加上自己里面还要调用Backtrace
std::string BacktraceToString(int size = 64, int skip = 2, const std::string &prefix = "");
}

// 当前时间，毫秒
uint64_t getCurrentTimeMS();
// 微妙
uint64_t getCurrentTimeUS();



#endif