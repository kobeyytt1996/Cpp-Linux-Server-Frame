#include "log.h"
#include <map>
#include <functional>
#include <time.h>
#include "config.h"
#include "log_config.h"

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
            return "UNKNOWN";
    }

    return "UNKNOWN";
}

LogLevel::Level LogLevel::FromString(const std::string &str) {
#define XX(level, innerStr) \
    if (str == #innerStr) { \
        return Level::level; \
    }
    // 配置文件里可能大写可能小写，都要处理
    XX(DEBUG, debug);
    XX(INFO, info);
    XX(WARN, warn);
    XX(ERROR, error);
    XX(FATAL, fatal);

    XX(DEBUG, DEBUG);
    XX(INFO, INFO);
    XX(WARN, WARN);
    XX(ERROR, ERROR);
    XX(FATAL, FATAL);
    return LogLevel::UNKNOWN;
#undef XX
}

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, int32_t line, uint32_t threadId, uint32_t fiberId
        , uint64_t time, uint32_t elapse, const std::string &thread_name)
    : m_file(file)
    , m_line(line)
    , m_threadId(threadId) 
    , m_threadName(thread_name)
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

// 多线程，且会有多个线程池，打印出Thread name能提示更多信息
class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatItem(const std::string &str = "") {}
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadName();
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
    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T<%f:%l>%T%m%n"));
}
void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        MutexType::Lock lock(m_mutex);
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
    MutexType::Lock lock(m_mutex);
    // shared_ptr重写了operator bool，因此直接判断其是否为空
    if (!appender->getFormatter()) {
        // 加锁时一定要小心死锁。appender->getFormatter()也对appender的锁加锁了，所以下面这行必须在if里面
        MutexType::Lock lock1(appender->m_mutex);
        // Logger是LogAppender的友元。不走appender的setFormatter方法，因为这里只是沿用logger的formatter，appender自己没有
        appender->m_formatter = m_formatter;
    }
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
        if (*it == appender) {
            m_appenders.erase(it);
            break;
        }
    }
}
void Logger::clearAppenders() {
    MutexType::Lock lock(m_mutex);
    m_appenders.clear();
}

void Logger::setFormatter(const std::string &format) {
    LogFormatter::ptr new_formatter(new LogFormatter(format));
    if (!new_formatter->isError()) {
        setFormatter(new_formatter);
    } else {
        std::cout << "Logger setFormatter name=" << m_name
                << " value=" << format << "invalid format" << std::endl;
        return;
    }
}
void Logger::setFormatter(LogFormatter::ptr formatter) {
    // 下面改appender里的formatter还有一把锁，注意要锁的东西是不一样的
    MutexType::Lock lock(m_mutex);
    m_formatter = formatter;

    // 有些Appender没有自己的Formatter，要跟着Logger进行变化
    for (auto &appender : m_appenders) {
        MutexType::Lock lock1(appender->m_mutex);
        if (!appender->m_hasFormatter) {
            appender->m_formatter = formatter;
        }
    }
}
LogFormatter::ptr Logger::getFormatter() const {
    return m_formatter;
}

std::string Logger::toYAMLString() {
    // 为了m_formatter和m_appenders的安全调用
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["name"] = m_name;
    if (m_level != LogLevel::UNKNOWN) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if (m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    for (auto &app : m_appenders) {
        node["appenders"].push_back(YAML::Load(app->toYAMLString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

/**
 * Appender及其各种子类的实现
 */

void LogAppender::setFormatter(const std::string &format) {
    LogFormatter::ptr new_formatter(new LogFormatter(format));
    if (!new_formatter->isError()) {
        setFormatter(new_formatter);
    } else {
        std::cout << "LogAppender setFormatter value=" << format << "invalid format" << std::endl;
        return;
    }
}

void LogAppender::setFormatter(LogFormatter::ptr formatter) { 
    // 设定的时候，别的线程可能在使用formatter，所以加速
    MutexType::Lock lock(m_mutex);
    if (formatter) {
        m_formatter = formatter; 
        m_hasFormatter = true;
    }
}

LogFormatter::ptr LogAppender::getFormatter() {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

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
        uint64_t now = time(nullptr);
        if (now - m_lastTime > 3) {
            reopen();
            m_lastTime = now;
        }
        // m_filestream和m_formatter都需要加锁
        MutexType::Lock lock(m_mutex);
        // 需考虑异常情况。输出过程中如果输出文件被删除了。shell里可以先ps aux拿到进程号，再ls -lh /proc/进程号/fd，看到所有fd的状态。
        // 需做兜底处理。这里的<<感知不到文件被删除（还需研究），故隔一段时间reopen一次
        m_filestream << m_formatter->format(logger, level, event);
    }
}

bool FileLogAppender::reopen() {
    MutexType::Lock lock(m_mutex);
    if (m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename);
    // io对象到bool的转换是explicit
    return static_cast<bool>(m_filestream);
}

std::string FileLogAppender::toYAMLString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = m_filename;
    if (m_level != LogLevel::UNKNOWN) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if (m_hasFormatter && m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        MutexType::Lock lock(m_mutex);
        std::cout << m_formatter->format(logger, level, event);
    }
}

std::string StdoutLogAppender::toYAMLString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    if (m_level != LogLevel::UNKNOWN) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if (m_hasFormatter && m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
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
    // 以下写法为了用map存储各种字符对应的不同FormatItem子类类型，注意不是存的子类对象，而是function，
    // 因为每次fmt可能会不同
    static std::map<std::string, std::function<FormatItem::ptr(const std::string &fmt)>> s_format_items = {
#define XX(type, C) \
        {#type, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); }}
        // 以下有些是按照log4j标准，有些是自己补充的，以这里的为准
        XX(m, MessageFormatItem),
        XX(p, LevelFormatItem),
        XX(r, ElapseFormatItem),
        XX(c, LoggerNameFormatItem),
        XX(t, ThreadIdFormatItem),
        XX(N, ThreadNameFormatItem),
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
        // 以下为调试内容。注意区分调试时的输出，上线前要去掉
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

    m_loggers[m_root->getName()] = m_root;

    init();
}

Logger::ptr LoggerManager::getLogger(const std::string &name) {
    MutexType::Lock lock(m_mutex);

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

std::string LoggerManager::toYAMLString() {
    MutexType::Lock lock(m_mutex);

    YAML::Node node;
    for (auto &logger : m_loggers) {
        node.push_back(YAML::Load(logger.second->toYAMLString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

void LoggerManager::init() {
}

// 下面开始是有关通过yaml配置改变log的约定
ConfigVar<std::set<LogDefine>>::ptr g_log_define = 
    Config::Lookup("logs", std::set<LogDefine>(), "logs config");

// 技巧：为了在main前做一些事情，可以定义类，然后把操作放到构造函数里，然后定义（static）全局变量
// yaml在解析log相关的配置后，回调这里，然后判断是否有增删改，相应修改之前的logger
struct LogIniter {
    LogIniter() {
        // 该键值为随机的
        g_log_define->add_listener([](const std::set<LogDefine> &old_val, const std::set<LogDefine> &new_val) {
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
                    if (!appenderDefine.formatter.empty()) {
                        appender->setFormatter(appenderDefine.formatter);
                    }
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
}