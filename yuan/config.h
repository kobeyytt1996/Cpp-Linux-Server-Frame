
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
protected:
    std::string m_name;
    std::string m_description;
};

// 负责简单类型和string的转换，用LexicalCast即可
template<typename From, class To>
class LexicalCast {
public:
    To operator()(const From &val) {
        return boost::lexical_cast<To>(val);
    }
};

// 重点。偏特化处理特殊类型的序列化和反序列化，都依赖YAML来完成(因为这里的string是yaml配置文件里的string)
template<typename T>
class LexicalCast<std::string, std::vector<T>> {
public:
    std::vector<T> operator()(const std::string &str) {
        YAML::Node node = YAML::Load(str);
        std::vector<T> vec;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            // 属于递归调用，T即可能是简单类型，也可能还是vector这种复杂类型
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

// 和上面的思路类似
template<typename T>
class LexicalCast<std::vector<T>, std::string> {
public:
    std::string operator()(const std::vector<T> &vec) {
        YAML::Node node;
        for (auto &t : vec) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(t)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// 具体的配置项。FromStr和ToStr即两个中间类(functor)，统一处理各种类型和string的转换。序列化和反序列化
// FromStr: T operator()(string str);  ToStr: string operator(const T &val);
template<typename T, typename FromStr = LexicalCast<std::string, T>
                    , typename ToStr = LexicalCast<T, std::string>>
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
    void setValue(const T &val) { m_val = val; }
private:
    T m_val;
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
        auto tmp = Lookup<T>(name);
        if (tmp) {
            YUAN_LOG_INFO(YUAN_GET_ROOT_LOGGER()) << "Lookup name=" << name << " exists";
            return tmp;
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