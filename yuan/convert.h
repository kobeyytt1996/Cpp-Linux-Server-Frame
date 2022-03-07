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
        // 下面的遍历可能因为yaml中格式的问题会抛出异常，但调用该转换的地方外层可以catch住
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            // 重点：属于递归调用，T即可能是简单类型，也可能还是vector这种复杂类型,或者自定义类型，都能支持
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

// set也是对应yaml配置文件中的数组，只不过具备了排序、去重等功能
template<typename T>
class LexicalCast<std::string, std::set<T>> {
public:
    std::set<T> operator()(const std::string &str) {
        YAML::Node node = YAML::Load(str);
        std::set<T> s;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            // 属于递归调用，T即可能是简单类型，也可能还是vector这种复杂类型
            s.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return s;
    }
};

template<typename T>
class LexicalCast<std::set<T>, std::string> {
public:
    std::string operator()(const std::set<T> &s) {
        YAML::Node node;
        for (auto &t : s) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(t)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<typename T>
class LexicalCast<std::string, std::unordered_set<T>> {
public:
    std::unordered_set<T> operator()(const std::string &str) {
        YAML::Node node = YAML::Load(str);
        std::unordered_set<T> s;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            // 属于递归调用，T即可能是简单类型，也可能还是vector这种复杂类型
            s.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return s;
    }
};

template<typename T>
class LexicalCast<std::unordered_set<T>, std::string> {
public:
    std::string operator()(const std::unordered_set<T> &s) {
        YAML::Node node;
        for (auto &t : s) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(t)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// map相较于前面要复杂一些,要特别注意。这里map的key支持string即够用了。
template<typename T>
class LexicalCast<std::string, std::map<std::string, T>> {
public:
    std::map<std::string, T> operator()(const std::string &str) {
        YAML::Node node = YAML::Load(str);
        std::map<std::string, T> m;
        std::stringstream ss;
        // 遍历方式改为把node认为是map的遍历方式
        for (auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it->second;
            // 先认为it->first是string类型
            m.insert({it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())});
        }
        return m;
    }
};

template<typename T>
class LexicalCast<std::map<std::string, T>, std::string> {
public:
    std::string operator()(const std::map<std::string, T> &m) {
        YAML::Node node;
        for (auto &t : m) {
            node[t.first] = YAML::Load(LexicalCast<T, std::string>()(t.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<typename T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
public:
    std::unordered_map<std::string, T> operator()(const std::string &str) {
        YAML::Node node = YAML::Load(str);
        std::unordered_map<std::string, T> m;
        std::stringstream ss;
        // 遍历方式改为把node认为是map的遍历方式
        for (auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it->second;
            // 先认为it->first是string类型
            m.insert({it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())});
        }
        return m;
    }
};

template<typename T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
public:
    std::string operator()(const std::unordered_map<std::string, T> &m) {
        YAML::Node node;
        for (auto &t : m) {
            node[t.first] = YAML::Load(LexicalCast<T, std::string>()(t.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

}

#endif