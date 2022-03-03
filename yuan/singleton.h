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
        // TODO: 线程安全吗？
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