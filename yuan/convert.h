/**
 * @file convert.h
 * @author your name (you@domain.com)
 * @brief 主要包含所有简单类型和复杂类型（包括stl中各种容器和自定义类型）的序列化和反序列化方法，都是依赖YAML来
 * 完成，主要是用在配置系统config.h中。
 * @version 0.1
 * @date 2022-03-06
 */

#ifndef __YUAN_CONVERT_H__
#define __YUAN_CONVERT_H__

#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

namespace yuan {

/**
 * @brief 以下都是各种简单类型及复杂类型的序列化和反序列化方法
 * 
 */
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

template<typename T>
class LexicalCast<std::string, std::list<T>> {
public:
    std::list<T> operator()(const std::string &str) {
        YAML::Node node = YAML::Load(str);
        std::list<T> l;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            // 属于递归调用，T即可能是简单类型，也可能还是vector这种复杂类型
            l.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return l;
    }
};

template<typename T>
class LexicalCast<std::list<T>, std::string> {
public:
    std::string operator()(const std::list<T> &l) {
        YAML::Node node;
        for (auto &t : l) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(t)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

}

#endif