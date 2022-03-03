#include "log.h"
#include <map>
#include <functional>
#include <time.h>

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
        os << logger->getName();
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
        auto self = shared_from_this();
        for (auto &appender : m_appenders) {
            appender->log(self, level, event);
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
            } else {
                m_items.push_back(it->second(std::get<1>(t)));
            }
        }
        // 以下为调试内容
        // std::cout << std::get<0>(t) << " - " << std::get<1>(t) << " - " << std::get<2>(t) << std::endl;
    }


}
/**
 * 以下为LogManager
 * 
 */
LogManager::LogManager() {
    m_root.reset(new Logger());
    m_root->addAppender(LogAppender::ptr(new StdoutLogAppender()));
}

Logger::ptr LogManager::getLogger(const std::string &name) const {
    auto it = m_loggers.find(name);
    if (it != m_loggers.end()) {
        return it->second;
    } else {
        return m_root;
    }
}
void LogManager::init() {

}
}