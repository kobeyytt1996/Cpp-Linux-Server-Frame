#ifndef __YUAN_ALL_HEADERS_H__
#define __YUAN_ALL_HEADERS_H__

// 所有头文件在这里好处是所有测试程序每次包含这个头文件就可以，坏处是修改一个头文件，所有测试程序都要重新编译
// 多个文件的最好按首字母排好，养成良好习惯
#include "config.h"
#include "fiber.h"
#include "log.h"
#include "macro.h"
#include "scheduler.h"
#include "singleton.h"
#include "thread.h"
#include "util.h"

#endif