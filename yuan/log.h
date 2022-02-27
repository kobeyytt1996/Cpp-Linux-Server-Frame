#ifndef __YUAN_LOG_H__
#define __YUAN_LOG_H__

#include <string>
#include <cstdint>
#include <memory>
#include <list>
#include <fstream>
#include <sstream>
#include <iostream>

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

    virtual void log(LogLevel::Level level, LogEvent::ptr event) = 0;

    void setFormatter(LogFormatter::ptr formatter) { m_formatter = formatter; }
    LogFormatter::ptr getFormatter() { return m_formatter; }
protected:
    LogLevel::Level m_level;
    LogFormatter::ptr m_formatter;
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
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    virtual void log(LogLevel::Level level, LogEvent::ptr event) override;
private:
};

// 日志输出到文件的Appender 
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string &filename);

    virtual void log(LogLevel::Level level, LogEvent::ptr event) override;

    // 重新打开文件，打开成功则返回true
    bool reopen();
private:
    std::string m_filename;
    std::ofstream m_filestream;
};

}





#endif