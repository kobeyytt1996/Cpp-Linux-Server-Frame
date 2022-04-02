#ifndef _http11_common_h
#define _http11_common_h
/**
 * 利用ragel来完成http文本的解析。ragel是有状态机实现的，有自己的语言，性能很高，甚至比汇编还快：https://blog.csdn.net/weixin_43798887/article/details/116100949
 * 这里用的是mongrel2库中对http解析的封装，底层就是用的ragel，并使用大量正则表达式：https://github.com/mongrel2/mongrel2/tree/master/src/http11
 * 把它的头文件和rl文件拷贝过来，那些.rl.cc文件都是rl文件生成出来的。这样命名来和我们自己写的源码区分
 * 安装ragel后，运行ragel -G2 -C rl文件名 -o 生成的源代码文件名
 */
#include <sys/types.h>

typedef void (*element_cb)(void *data, const char *at, size_t length);
typedef void (*field_cb)(void *data, const char *field, size_t flen, const char *value, size_t vlen);

#endif