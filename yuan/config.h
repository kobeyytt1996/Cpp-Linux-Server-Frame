
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

namespace yuan {

// 配置项的基类
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;
    ConfigVarBase(const std::string &name, const std::string &description = "")
        : m_name(name), m_description(description) {}

    virtual ~ConfigVarBase() {}

    const std::string &getName() const { return m_name; }
    const std::string &getDescription() const { return m_description; }

    virtual std::string toString() const = 0;
    virtual bool fromString(const std::string &val) = 0;
protected:
    std::string m_name;
    std::string m_description;
};

// 具体的配置项
template<typename T>
class ConfigVar : public ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVar> ptr;
    ConfigVar(const std::string &name
            , const T &default_value
            , const std::string &description = "")
        : ConfigVarBase(name, description)
        , m_val(default_value) {}

    virtual std::string toString() const override {
        try {   
            return boost::lexical_cast<std::string>(m_val);
        } catch (std::exception &e) {
            YUAN_LOG_ERROR(YUAN_GET_ROOT_LOGGER()) << "ConfigVar::toString exception "
                << e.what() << " convert: " << typeid(T).name() << " to string";
        }
        return "";
    }

    virtual bool fromString(const std::string &val) override {
        try {
            m_val = boost::lexical_cast<T>(val);
            return true;
        } catch (std::exception &e) {
            YUAN_LOG_ERROR((YUAN_GET_ROOT_LOGGER())) << "ConfigVar::fromString exception "
                << e.what() << " convert string:" << val << " to " << typeid(T).name();
        }

        return false;
    }

    const T getValue() const { return m_val; }
    void setValue(const T &val) { m_val = val; }
private:
    T m_val;
};

/**
 * @brief 用map存储所有ConfigVar对象
 * 
 */
class Config {
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;

    // 查找，如果不存在则插入。通过该方法可以让用户方便的获取到配置项
    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string &name
            , const T &default_value, const std::string &description) {
        auto tmp = Lookup<T>(name);
        if (tmp) {
            YUAN_LOG_INFO(YUAN_GET_ROOT_LOGGER()) << "Lookup name=" << name << " exists";
            return tmp;
        }
        if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._0123456789") != std::string::npos) {
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
private:
    static ConfigVarMap s_datas;
};

}

#endif