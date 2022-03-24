#ifndef __NONCOPYABLE_H__
#define __NONCOPYABLE_H__
/**
 * 定义不可复制的类，需要这种特性的类就继承该类
 */
namespace yuan {

class Noncopyable {
public:
    // 默认构造函数必须定义，下面拷贝构造函数尽管已经声明为delete，编译器也认为它定义了，则不再生成默认构造函数
    Noncopyable() = default;
    ~Noncopyable() = default;
    Noncopyable(const Noncopyable &) = delete;
    Noncopyable &operator=(const Noncopyable &) = delete;
};

}

#endif