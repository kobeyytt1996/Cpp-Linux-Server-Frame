
/**
 * 这个头文件都是有关配置系统中设定log相关的部分
 */
#ifndef __YUAN_LOG_CONFIG_H__
#define __YUAN_LOG_CONFIG_H__

#include "config.h"
#include "log.h"
namespace yuan {

/**
 * @brief 以下两个类作为配置系统中日志器和日志输出地的自定义类，看log.yml辅助理解。
 * 在struct里data members 不应该用m开头
 */
struct LogAppenderDefine {
    // 1 File 2 Stdout
    int type = 0;
    LogLevel::Level level = LogLevel::UNKNOWN;
    std::string formatter;
    std::string file;

    // 因为ConfigVar里判断值是否变化并通知监听者时用到了比较
    bool operator==(const LogAppenderDefine &rhs) const {
        return type == rhs.type && level == rhs.level
                && formatter == rhs.formatter && file == rhs.file;
    }
};

struct LogDefine {
    std::string name;
    LogLevel::Level level = LogLevel::UNKNOWN;
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;

    bool operator==(const LogDefine &rhs) const {
        return name == rhs.name && level == rhs.level
                && formatter == rhs.formatter && appenders == rhs.appenders;
    }

    bool operator!=(const LogDefine &rhs) const {
        return !operator==(rhs);
    }

    // 下面使用set来去重，因此要定义<
    bool operator<(const LogDefine &rhs) const {
        return name < rhs.name;
    }
};

// 对自定义类的序列化和反序列化，使用全特化的方式
template<>
class LexicalCast<std::string, std::set<LogDefine>> {
public:
    std::set<LogDefine> operator()(const std::string &str) {
        YAML::Node root = YAML::Load(str);
        std::set<LogDefine> logSet;
        for (decltype(root.size()) i = 0; i < root.size(); ++i) {
            YAML::Node node = root[i];
            if (!node["name"].IsDefined()) {
                std::cout << "log config error: name is null, " << node << std::endl;
                continue;
            }

            LogDefine ld;
            ld.name = node["name"].as<std::string>();
            ld.level = LogLevel::FromString(
                                    node["level"].IsDefined() ? node["level"].as<std::string>() : "");
            if (node["formatter"].IsDefined()) {
                ld.formatter = node["formatter"].as<std::string>();
            }
            if (node["appenders"].IsDefined()) {
                for (size_t j = 0; j < node["appenders"].size(); ++j) {
                    auto appenderNode = node["appenders"][j];
                    if (!appenderNode["type"].IsDefined()) {
                        std::cout << "log config error: appender type is null, " << appenderNode << std::endl;
                        continue;
                    }
                    std::string type = appenderNode["type"].as<std::string>();
                    LogAppenderDefine lad;
                    if (type == "FileLogAppender") {
                        lad.type = 1;
                        if (!appenderNode["file"].IsDefined()) {
                            std::cout << "log config error: fileLogAppender file is null, " << appenderNode << std::endl;
                            continue;
                        }
                        lad.file = appenderNode["file"].as<std::string>();
                    } else if (type == "StdoutLogAppender") {
                        lad.type = 2;
                    } else {
                        std::cout << "log config error: appender type invalid, " << appenderNode << std::endl;
                        continue;
                    }
                    if (appenderNode["formatter"].IsDefined()) {
                        lad.formatter = appenderNode["formatter"].as<std::string>();
                    }
                    ld.appenders.push_back(lad);
                }
            }
            logSet.insert(ld);
        }
        
        return logSet;
    }
};

// 和上面的思路类似
template<>
class LexicalCast<std::set<LogDefine>, std::string> {
public:
    std::string operator()(const std::set<LogDefine> &logSet) {
        YAML::Node node;
        for (auto &log : logSet) {
            YAML::Node logNode;
            logNode["name"] = log.name;
            if(log.level != LogLevel::UNKNOWN) {
                logNode["level"] = LogLevel::ToString(log.level);
            }
            if (!log.formatter.empty()) {
                logNode["formatter"] = log.formatter;
            }
            for (auto &appender : log.appenders) {
                YAML::Node appNode;
                if (appender.type == 1) {
                    appNode["type"] = "FileLogAppender";
                    appNode["file"] = appender.file;
                } else if (appender.type == 2) {
                    appNode["type"] = "StdoutLogAppender";
                }
                if(log.level != LogLevel::UNKNOWN) {
                    appNode["level"] = LogLevel::ToString(appender.level);
                }
                if (!appender.formatter.empty()) {
                    appNode["formatter"] = appender.formatter;
                }
                logNode["appenders"].push_back(appNode);
            }
            node.push_back(logNode);
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

}

#endif