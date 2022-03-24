#ifndef __YUAN_SINGLETON_H__
#define __YUAN_SINGLETON_H__
#include <memory>

namespace yuan {

// 使用模板方式增加两个单例类

// N是为了防止要多个实例
template<typename T, typename X = void, int N = 0>
class Singleton {
public:
    static T* GetInstance() {
        // 线程安全吗？可以保证只有一个线程定义初始化该变量，但依旧线程不安全：https://blog.csdn.net/firetoucher/article/details/1767318
        static T instance;
        return &instance;
    }
};

template<typename T, typename X = void, int N = 0>
class SingletonPtr {
public:
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> instance(new T());
        return instance;
    }
};


}

#endif