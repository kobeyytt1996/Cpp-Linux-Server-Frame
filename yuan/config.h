
#ifndef __YUAN_CONFIG_H__
#define __YUAN_CONFIG_H__

/**
 * @file config.h
 * @author Yuan Yuan
 * @brief 用于配置，定义了约定项的类，和存储约定项的管理类，以便加载配置文件来修改相应的约定项
 * @version 0.1
 * @date 2022-03-03
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <memory>
#include <sstream>
#include <string>
// 可以很方便的类型转换的库:https://www.boost.org/doc/libs/1_44_0/libs/conversion/lexical_cast.htm
#include <boost/lexical_cast.hpp>
#include "log.h"
#include <yaml-cpp/yaml.h>
// 定义各种类序列化和反序列化为yaml格式的头文件
#include "convert.h"
#include <functional>
#include "thread.h"

namespace yuan {

// 约定项的基类 
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    // 和日志系统不同，配置系统这里读多写少。故用读写锁
    typedef RWMutex RWMutexType;

    ConfigVarBase(const std::string &name, const std::string &description = "")
        : m_name(name)
        , m_description(description) {
            // 把所有字母转为小写，方便使用
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
        }

    virtual ~ConfigVarBase() {}

    const std::string &getName() const { return m_name; }
    const std::string &getDescription() const { return m_description; }

    // 以下两个方法最为核心，因为yaml等配置里解析出来的是string，因此一定会涉及存储的value的序列化和反序列化
    virtual std::string toString() = 0;
    virtual bool fromString(const std::string &val) = 0;
    virtual std::string getTypename() const = 0;
protected:
    std::string m_name;
    std::string m_description;
    RWMutexType m_mutex;
};

// 具体的约定项。FromStr和ToStr即两个中间类(functor)，统一处理各种类型和string的转换。序列化和反序列化
// FromStr: T operator()(string str);  ToStr: string operator(const T &val);
template<typename T, typename FromStr = LexicalCast<std::string, T>
                    , typename ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVar> ptr;
    // 重点：在加载yaml配置文件后，如果某项约定发生修改，可以回调来通知旧值和新值。
    typedef std::function<void (const T &old_value, const T &new_value)> on_change_cb;

    ConfigVar(const std::string &name
            , const T &default_value
            , const std::string &description = "")
        : ConfigVarBase(name, description)
        , m_val(default_value) {}

    virtual std::string toString() override {
        try { 
            // 先支持简单基础类型的value，所以用boost库的转换就够了  
            // return boost::lexical_cast<std::string>(m_val);
            // 细节：不要直接用m_val,要保证封装性，而且更好管理，比如在getValue里加锁，这里就不用加了
            return ToStr()(getValue());
        } catch (std::exception &e) {
            YUAN_LOG_ERROR(YUAN_GET_ROOT_LOGGER()) << "ConfigVar::toString exception "
                << e.what() << " convert: " << typeid(T).name() << " to string";
        }
        return "";
    }

    virtual bool fromString(const std::string &val) override {
        try {
            // m_val = boost::lexical_cast<T>(val);
            setValue(FromStr()(val));
            return true;
        } catch (std::exception &e) {
            YUAN_LOG_ERROR((YUAN_GET_ROOT_LOGGER())) << "ConfigVar::fromString exception "
                << e.what() << " convert string:" << val << " to " << typeid(T).name();
        }

        return false;
    }

    const T getValue() { 
        RWMutexType::ReadLock lock(m_mutex);
        return m_val;
    }
    void setValue(const T &val) { 
        // 细节：下面分别加了读锁和写锁。注意加了作用域，来让读锁使用后被释放。降低死锁概率
        {
            RWMutexType::ReadLock lock(m_mutex);
            if (val == m_val) {
                return;
            }
            // 在改变值的时候调用回调函数
            for (auto &cb : m_cbs) {
                cb.second(m_val, val);
            }
        }

        RWMutexType::WriteLock lock(m_mutex);
        m_val = val;
     }
    std::string getTypename() const override { return typeid(T).name(); }

    uint64_t add_listener(/*uint64_t key, */on_change_cb cb) {
        RWMutexType::WriteLock lock(m_mutex);
        // 原本去重是让使用者控制键值，使用者来传入，但这样并不优雅，使用者不确定哪些key被占用，因此改为下面这种，把key返还给使用者。
        static uint64_t s_fun_key = 0;
        ++s_fun_key;
        m_cbs[s_fun_key] = cb;
        return s_fun_key;
    }
    void del_listener(uint64_t key) {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.erase(key);
    }
    void clear_listener() {
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.clear();
    }
    on_change_cb getListener(uint64_t key) {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_cbs.find(key);
        if (it == m_cbs.end()) {
            return nullptr;
        } else {
            return it->second;
        }
    }
private:
    T m_val;
    // 变更回调函数集合。为什么用map？因为function没有比较函数，放在vector里无法移除某个指定function，故使用map。
    // uint64_t key 要求唯一，一般可以使用hash
    std::map<uint64_t, on_change_cb> m_cbs;
};

/**
 * @brief 让用户增加、查找约定项的管理类，并能加载yaml配置文件，修改相同名字(key值)的约定项
 * 用map存储所有ConfigVarBase对象。
 */
class Config {
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;
    typedef RWMutex RWMutexType;

    // 查找，如果不存在则插入。通过该方法可以让用户方便的添加约定项
    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string &name
            , const T &default_value, const std::string &description) {
        // 不再使用下面这种方法查找，因为返回nullptr有两种可能，一是值不存在，二是存在但其value与T类型不同
        // auto tmp = Lookup<T>(name);
        auto it = GetDatas().end();
        {
            RWMutexType::ReadLock lock(GetMutex());
            it = GetDatas().find(name);
        }
        if (it != GetDatas().end()) {
            typename ConfigVar<T>::ptr tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
            if (tmp) {
                YUAN_LOG_INFO(YUAN_GET_ROOT_LOGGER()) << "Lookup name=" << name << " exists";
                return tmp;
            } else {
                // 说明键一样，但值类型不同，要报错
                YUAN_LOG_ERROR(YUAN_GET_ROOT_LOGGER()) << "Lookup name=" << name 
                    << " exists but type not: " << typeid(T).name() << " real_type=" << it->second->getTypename();
                return nullptr;
            }
        }
        if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
            YUAN_LOG_ERROR((YUAN_GET_ROOT_LOGGER())) << "Lookup name invalid:" << name;
            throw std::invalid_argument(name);
        }

        typename ConfigVar<T>::ptr configVar(new ConfigVar<T>(name, default_value, description));
        RWMutexType::WriteLock lock(GetMutex());
        GetDatas()[name] = configVar;
        return configVar;
    }

    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string &name) {
        RWMutexType::ReadLock lock(GetMutex());
        auto it = GetDatas().find(name);
        if (it == GetDatas().end()) {
            return nullptr;
        } else {
            // dynamic_pointer_cast是dynamic_cast的智能指针版本
            return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
        }
    }

    static ConfigVarBase::ptr LookupBase(const std::string &name);

    static void LoadFromYaml(const YAML::Node &root);
    // 方便调试时使用，可以让使用者看到配置系统里目前都有什么约定。传入回调
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);
private:
    // 不能像下面这样定义s_datas，因为有些静态函数使用到它时，它可能还没有初始化。定义s_datas和使用s_datas的源文件执行先后顺序不确定
    // static ConfigVarMap s_datas;
    // 为避免上述问题，采用下面方法，具体可查Effective C++（注意返回的一定是引用，否則不能保证唯一性）
    static ConfigVarMap &GetDatas() {
        static ConfigVarMap s_datas;
        return s_datas;
    }

    static RWMutexType &GetMutex() {
        static RWMutexType s_mutex;
        return s_mutex;
    }
};

}

#endif