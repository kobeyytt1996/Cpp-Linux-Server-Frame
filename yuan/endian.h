#ifndef __YUAN_ENDIAN_H__
#define __YUAN_ENDIAN_H__
/**
 * 用于字节序转换的工具类
 * 
 */

#define YUAN_LITTLE_ENDIAN 1
#define YUAN_BIG_ENDIAN 2

#include <byteswap.h>
#include <stdint.h>
#include <type_traits>

namespace yuan {

// 以下三个是转换字节序的函数
// 重点：模板函数的重载有和普通函数的有区别：https://blog.csdn.net/u012481976/article/details/84502874
// 重点：学习enable_if的使用:https://ouuan.github.io/post/c-11-enable-if-%E7%9A%84%E4%BD%BF%E7%94%A8/
template<typename T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
byteswap(T value) {
    // bswap即进行字节序反转：https://man7.org/linux/man-pages/man3/bswap.3.html
    return (T)bswap_64((uint64_t)value);
}

template<typename T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
byteswap(T value) {
    return (T)bswap_32((uint32_t)value);
}

template<typename T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
byteswap(T value) {
    return (T)bswap_16((uint16_t)value);
}

// 都是系统已定义好的宏
#if BYTE_ORDER == BIG_ENDIAN
#define YUAN_BYTE_ORDER YUAN_BIG_ENDIAN
#else
#define YUAN_BYTE_ORDER YUAN_LITTLE_ENDIAN
#endif

// 根据编译环境的情况定义对应的函数
#if YUAN_BYTE_ORDER == YUAN_BIG_ENDIAN
// 调用这个函数意思是想要转成大端字节序。函数名字的意思是小端上才需要实际的转换。因此只在小端的机器上转换，大端机器上什么都不做
template<typename T>
T byteswapOnLittleEndian(T t) {
    return t;
}

template<typename T>
T byteswapOnBigEndian(T t) {
    return byteswap(t);
}

#else

template<typename T>
T byteswapOnLittleEndian(T t) {
    return byteswap(t);
}

template<typename T>
T byteswapOnBigEndian(T t) {
    return t;
}

#endif

}

#endif