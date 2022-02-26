#ifndef __YUAN_LOG_H__
#define __YUAN_LOG_H__

#include <string>
#include <cstdint>
#include <memory>
#include <list>

namespace yuan {

class LogEvent {
public:
    using ptr = std::shared_ptr<LogEvent>;
    LogEvent();
private:
    // 文件名
    const char * m_file = nullptr; 
    // 行号
    int32_t m_line = 0;
    uint32_t m_threadId = 0;
    // 协程ID
    uint32_t m_fiberId = 0;
    // 时间戳
    uint64_t m_time = 0;
    // 程序启动到现在的毫秒数
    uint32_t m_elapse = 0;
    std::string m_content;
};

class LogLevel {
public:
    enum Level {
        DEBUG = 1,
        INFO,
        WARN,
        ERROR,
        FATAL
    };
};

// 日志格式器
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;

    std::string format(LogEvent::ptr event);
private:
};

// 日志输出地
class LogAppender {
public:
    typedef std::shared_ptr<LogAppender> ptr;
    virtual ~LogAppender();

    void log(LogLevel::Level level, LogEvent::ptr event);
private:
    LogLevel::Level m_level;
};

// 日志器
class Logger {
public:
    typedef std::shared_ptr<Logger> ptr;

    Logger(const std::string &name = "root");
    void log(LogLevel::Level level, LogEvent::ptr event);

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    LogLevel::Level getLevel() const { return m_level; }
    void setLevel(LogLevel::Level level) { m_level = level; }
private:
    // 日志名称
    std::string m_name;
    // 满足该日志级别才会输出
    LogLevel::Level m_level;
    // Appender集合
    std::list<LogAppender::ptr> m_appenders;
};

// 日志输出到控制台的Appender
class StdoutLogAppender : public LogAppender {

};

// 日志输出到文件的Appender 
class FileLogAppender : public LogAppender {

};

}





#endif