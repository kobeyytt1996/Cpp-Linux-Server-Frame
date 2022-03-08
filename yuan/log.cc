#include "log.h"
#include <map>
#include <functional>
#include <time.h>
#include "config.h"

namespace yuan {

const char *LogLevel::ToString(Level level) {
    switch (level) {
    // 以下使用C宏，可参考https://www.runoob.com/cprogramming/c-preprocessors.html
#define XX(name) \
        case LogLevel::name: \
            return #name; \
            break;
        XX(DEBUG);
        XX(INFO);
        XX(WARN);
        XX(ERROR);
        XX(FATAL);
#undef XX
        default:
            return "UNKNOW";
    }

    return "UNKNOWN";
}

LogLevel::Level LogLevel::FromString(const std::string &str) {
#define XX(name) \
    if (str == #name) { \
        return Level::name; \
    }

    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
    return LogLevel::UNKNOWN;
#undef XX
}

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, int32_t line, uint32_t threadId, uint32_t fiberId
        , uint64_t time, uint32_t elapse)
    : m_file(file)
    , m_line(line)
    , m_threadId(threadId) 
    , m_fiberId(fiberId)
    , m_time(time)
    , m_elapse(elapse)
    , m_logger(logger)
    , m_level(level) {}

void LogEvent::format(const char *fmt, ...) {
    va_list al;
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}
void LogEvent::format(const char *fmt, va_list al) {
    char *buf = nullptr;
    // 改方法会给buf malloc空间，所以后面要free
    int len = vasprintf(&buf, fmt, al);
    if (len != -1) {
        m_ss << std::string(buf, len);
        free(buf);
    }
}

LogEventWrap::LogEventWrap(LogEvent::ptr event) : m_event(event) {

}
LogEventWrap::~LogEventWrap() {
    m_event->getLogger()->log(m_event->getLevel(), m_event);
}


/**
 * 以下各项FormatItem的实现是针对log4j的日志格式https://blog.csdn.net/ctwy291314/article/details/83822254
 * 中的各项的具体实现
 */

class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getContent();
    }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << LogLevel::ToString(level);
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getElapse();
    }
};

// 日志器的名称
class LoggerNameFormatItem : public LogFormatter::FormatItem {
public:
    LoggerNameFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        // 不能直接用logger，因为传入的logger可能是实际logger里的m_root，当实际logger没有配置时，m_root为兜底方案
        os << event->getLogger()->getName();
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadId();
    }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFiberId();
    }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    explicit DateTimeFormatItem(const std::string &format) : m_format(format) {
        if (format.empty()) {
            m_format = "%D";
        }
    }
    // 时间应根据pattern里面的%t{fff}的fff来格式化
    // 具体日期格式：https://blog.csdn.net/clarkness/article/details/90047406
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        struct tm tm;
        time_t eventTime = event->getTime();
        localtime_r(&eventTime, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }
private:
    std::string m_format;
};

class FilenameFormatItem : public LogFormatter::FormatItem {
public:
    FilenameFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFile();
    }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLine();
    }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << std::endl;
    }
};

class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(const std::string &str)
        : m_string(str) {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << m_string;
    }
private:
    std::string m_string;
};

// log4j里没有tab的格式，而又比较常用，所以增加一个，对应时用T对应，和线程分开
class TabFormatItem : public LogFormatter::FormatItem {
public:
    TabFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << '\t';
    }
};

/**
 * Logger的实现
 */

Logger::Logger(const std::string &name) : m_name(name), m_level(LogLevel::DEBUG) {
    // 默认的日志输出格式
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T<%f:%l>%T%m%n"));
}
void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        // 如果用户并没有对该logger进行配置，则走默认root的日志行为
        if (!m_appenders.empty()) {
            auto self = shared_from_this();
            for (auto &appender : m_appenders) {
                appender->log(self, level, event);
            }
        } else {
            m_root->log(level, event);
        }    
    }
}

// 以下几个方法都调用log，但更方便使用
void Logger::debug(LogEvent::ptr event) {
    log(LogLevel::DEBUG, event);
}
void Logger::info(LogEvent::ptr event) {
    log(LogLevel::INFO, event);
}
void Logger::warn(LogEvent::ptr event) {
    log(LogLevel::WARN, event);
}
void Logger::error(LogEvent::ptr event) {
    log(LogLevel::ERROR, event);
}
void Logger::fatal(LogEvent::ptr event) {
    log(LogLevel::FATAL, event);
}

void Logger::addAppender(LogAppender::ptr appender) {
    // shared_ptr重写了operator bool，因此直接判断其是否为空
    if (!appender->getFormatter()) {
        appender->setFormatter(m_formatter);
    }
    m_appenders.push_back(appender);
}
void Logger::delAppender(LogAppender::ptr appender) {
    for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
        if (*it == appender) {
            m_appenders.erase(it);
            break;
        }
    }
}
void Logger::clearAppenders() {
    m_appenders.clear();
}

void Logger::setFormatter(const std::string &format) {
    LogFormatter::ptr new_formatter(new LogFormatter(format));
    if (!new_formatter->isError()) {
        m_formatter = new_formatter;
    } else {
        std::cout << "Logger setFormatter name=" << m_name
                << " value=" << format << "invalid format" << std::endl;
        return;
    }
}
void Logger::setFormatter(LogFormatter::ptr formatter) {
    m_formatter = formatter;
}
LogFormatter::ptr Logger::getFormatter() const {
    return m_formatter;
}

/**
 * Appender及其各种子类的实现
 */

FileLogAppender::FileLogAppender(const std::string &filename) : m_filename(filename) {
    if (!reopen()) {
        std::cout << "文件打开失败" << std::endl;
    }
}

FileLogAppender::~FileLogAppender() {
    if (m_filestream) {
        m_filestream.close();
    }
}


void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        m_filestream << m_formatter->format(logger, level, event);
    }
}

bool FileLogAppender::reopen() {
    if (m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename);
    // io对象到bool的转换是explicit
    return static_cast<bool>(m_filestream);
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        std::cout << m_formatter->format(logger, level, event);
    }
}

/**
 * Formatter的实现
 */
LogFormatter::LogFormatter(const std::string &pattern) : m_pattern(pattern) {
    init();
}

// 输出到ostringstream中，再转string
std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    std::ostringstream oss;
    for (auto &item : m_items) {
        item->format(oss, logger, level, event);
    }
    return oss.str();
}

// 参照log4j的日志输出格式：https://blog.csdn.net/ctwy291314/article/details/83822254
// m_pattern设置为上述格式的组合，则可以控制输出日志的格式
// 可以分为三种：%xxx %xxx{fff} %%  （第二种中的{}描述了该项的不同输出格式，如时间有多种输出格式）
void LogFormatter::init() {
    // str, format, type
    std::vector<std::tuple<std::string, std::string, int>> vec;
    // normal string:除了上述格式以外想输出的内容
    std::string nstr;

    for (size_t i = 0; i < m_pattern.size(); ++i) {
        if (m_pattern[i] != '%') {
            nstr.append(1, m_pattern[i]);
            continue;
        }
        // 处理%%
        if (i + 1 < m_pattern.size() && (m_pattern[i+1] == '%')) {
            nstr.append(1, '%');
            continue;
        }

        size_t n = i + 1;
        // 有点类似有限状态机，fmt指%xxx{fff}中的fff, str指xxx
        int fmt_status = 0;
        size_t fmt_begin = 0;
        
        std::string str, fmt;
        while (n < m_pattern.size()) {
            // 进入{}前遇到非字符或非'{'均认为不再是%的一部分
            if (!fmt_status && !isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}') {
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            } else if (fmt_status == 0) {
                if (m_pattern[n] == '{') {
                    str = m_pattern.substr(i+1, n-i-1);
                    fmt_status = 1;
                    fmt_begin = n + 1;
                    ++n;
                    continue;
                }
            } else if (fmt_status == 1) {
                if (m_pattern[n] == '}') {
                    fmt = m_pattern.substr(fmt_begin, n - fmt_begin);
                    fmt_status = 0;
                    ++n;
                    break;
                }
            }
            ++n;
            if (n == m_pattern.size()) {
                if (str.empty()) {
                    str = m_pattern.substr(i+1);
                }
            }
        }

        if (!nstr.empty())
        {
            vec.push_back(std::make_tuple(nstr, "", 0));
            nstr.clear();
        }
        if (fmt_status == 0) {
            vec.push_back({str, fmt, 1});
            i = n - 1;
        } else if (fmt_status == 1) {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            vec.push_back({"<<pattern_error>>", fmt, 0});
            m_error = true;
            i = n;
        } 
    }
    // 处理最后一个nstr
    if (!nstr.empty()) {
        vec.push_back(std::make_tuple(nstr, "", 0));
    }
    // 以下写法为了用map存储各种字符对应的不同FormatItem子类类型，注意不是存的子类对象，而是function，因为每次fmt
    // 可能会不同
    static std::map<std::string, std::function<FormatItem::ptr(const std::string &fmt)>> s_format_items = {
#define XX(type, C) \
        {#type, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); }}
        XX(m, MessageFormatItem),
        XX(p, LevelFormatItem),
        XX(r, ElapseFormatItem),
        XX(c, LoggerNameFormatItem),
        XX(t, ThreadIdFormatItem),
        XX(n, NewLineFormatItem),
        XX(d, DateTimeFormatItem),
        XX(f, FilenameFormatItem),
        XX(l, LineFormatItem),
        XX(T, TabFormatItem),
        XX(F, FiberIdFormatItem)

#undef XX
    };

    for (auto &t : vec) {
        if (std::get<2>(t) == 0) {
            m_items.push_back(StringFormatItem::ptr(new StringFormatItem(std::get<0>(t))));
        } else {
            auto it = s_format_items.find(std::get<0>(t));
            if (it == s_format_items.end()) {
                m_items.push_back(StringFormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(t) + ">>")));
                m_error = true;
            } else {
                m_items.push_back(it->second(std::get<1>(t)));
            }
        }
        // 以下为调试内容
        // std::cout << std::get<0>(t) << " - " << std::get<1>(t) << " - " << std::get<2>(t) << std::endl;
    }


}
/**
 * LoggerManager
 * 
 */
LoggerManager::LoggerManager() {
    m_root.reset(new Logger());
    m_root->addAppender(LogAppender::ptr(new StdoutLogAppender()));

    init();
}

Logger::ptr LoggerManager::getLogger(const std::string &name) {
    auto it = m_loggers.find(name);
    if (it != m_loggers.end()) {
        return it->second;
    } 
    // 不存在则创建一个logger
    Logger::ptr logger(new Logger(name));
    logger->m_root = m_root;
    m_loggers[name] = logger;
    return logger;
}

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
            logNode["level"] = LogLevel::ToString(log.level);
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
                appNode["level"] = LogLevel::ToString(appender.level);
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


ConfigVar<std::set<LogDefine>>::ptr g_log_define = 
    Config::Lookup("logs", std::set<LogDefine>(), "logs config");

// 技巧：为了在main前做一些事情，可以定义类，然后把操作放到构造函数里，然后定义（static）全局变量
struct LogIniter {
    LogIniter() {
        // 该键值为随机的
        g_log_define->add_listener(0xF1E231, 
                [](const std::set<LogDefine> &old_val, const std::set<LogDefine> &new_val) {
            YUAN_LOG_INFO(YUAN_GET_ROOT_LOGGER()) << "on_logger_conf_changed";
            // 新增，修改，删除
            for (auto &logDefine : new_val) {
                auto it = old_val.find(logDefine);
                Logger::ptr logger;
                if (it == old_val.end()) {
                    // 新增
                    logger = YUAN_GET_LOGGER(logDefine.name);
                } else if (*it != logDefine) {
                    // 修改
                    logger = YUAN_GET_LOGGER(logDefine.name);
                }
                if (!logger) {
                    continue;
                }

                logger->setLevel(logDefine.level);
                if (!logDefine.formatter.empty()) {
                    logger->setFormatter(logDefine.formatter);
                }
                logger->clearAppenders();
                for (auto &appenderDefine : logDefine.appenders) {
                    LogAppender::ptr appender;
                    if (appenderDefine.type == 1) {
                        appender.reset(new FileLogAppender(appenderDefine.file));
                    } else if (appenderDefine.type == 2) {
                        appender.reset(new StdoutLogAppender());
                    }
                    appender->setLevel(appenderDefine.level);
                    if (appender) {
                        logger->addAppender(appender);
                    }
                }
            }

            // 删除
            for (auto &logDefine : old_val) {
                auto it = new_val.find(logDefine);
                if (it == new_val.end()) {
                    // 配置里删除了，我们这里不能真的删除，防止想再加回来时某些配置破坏。只要确保不再写日志即可
                    auto logger = YUAN_GET_LOGGER(logDefine.name);
                    // 日志级别设置的很高，则不会输出
                    logger->setLevel(static_cast<LogLevel::Level>(100));
                    // 清空appenders，则使用m_root
                    logger->clearAppenders();
                }
            }

        });
    }
};
// 技巧：为了在main前做一些事情，可以定义变量，然后把操作放到构造函数里
// static的用法：https://www.runoob.com/w3cnote/cpp-static-usage.html
static LogIniter __log_init;

void LoggerManager::init() {

}
}