
#ifndef __YUAN_CONFIG_H__
#define __YUAN_CONFIG_H__

/**
 * @file config.h
 * @author Yuan Yuan
 * @brief 用于对日志系统进行配置
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
#include "convert.h"
#include <functional>

namespace yuan {

// 配置项的基类 
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
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
    virtual std::string toString() const = 0;
    virtual bool fromString(const std::string &val) = 0;
    virtual std::string getTypename() const = 0;
protected:
    std::string m_name;
    std::string m_description;
};

// 具体的配置项。FromStr和ToStr即两个中间类(functor)，统一处理各种类型和string的转换。序列化和反序列化
// FromStr: T operator()(string str);  ToStr: string operator(const T &val);
template<typename T, typename FromStr = LexicalCast<std::string, T>
                    , typename ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVar> ptr;
    // 重点：在配置中某项发生修改，可以回调来通知旧值和新值。
    typedef std::function<void (const T &old_value, const T &new_value)> on_change_cb;

    ConfigVar(const std::string &name
            , const T &default_value
            , const std::string &description = "")
        : ConfigVarBase(name, description)
        , m_val(default_value) {}

    virtual std::string toString() const override {
        try { 
            // 先支持简单基础类型的value，所以用boost库的转换就够了  
            // return boost::lexical_cast<std::string>(m_val);
            return ToStr()(m_val);
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

    const T getValue() const { return m_val; }
    void setValue(const T &val) { 
        if (val == m_val) {
            return;
        }
        for (auto &cb : m_cbs) {
            cb.second(m_val, val);
        }
        m_val = val;
     }
    std::string getTypename() const override { return typeid(T).name(); }

    void add_listener(uint64_t key, on_change_cb cb) {
        m_cbs[key] = cb;
    }
    void del_listener(uint64_t key) {
        m_cbs.erase(key);
    }
    void clear_listener() {
        m_cbs.clear();
    }
    on_change_cb getListener(uint64_t key) {
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
 * @brief 用map存储所有ConfigVarBase对象
 */
class Config {
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;

    // 查找，如果不存在则插入。通过该方法可以让用户方便的获取到配置项
    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string &name
            , const T &default_value, const std::string &description) {
        // 不再使用下面这种方法查找，因为返回nullptr有两种可能，一是值不存在，二是存在但其value与T类型不同
        // auto tmp = Lookup<T>(name);
        auto it = s_datas.find(name);
        if (it != s_datas.end()) {
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
        s_datas[name] = configVar;
        return configVar;
    }

    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string &name) {
        auto it = s_datas.find(name);
        if (it == s_datas.end()) {
            return nullptr;
        } else {
            // dynamic_pointer_cast是dynamic_cast的智能指针版本
            return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
        }
    }

    static ConfigVarBase::ptr LookupBase(const std::string &name);

    static void LoadFromYaml(const YAML::Node &root);
private:
    static ConfigVarMap s_datas;
};

}

#endif