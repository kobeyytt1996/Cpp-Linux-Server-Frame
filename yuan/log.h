#ifndef __YUAN_LOG_H__
#define __YUAN_LOG_H__

#include <string>
#include <cstdint>
#include <memory>
#include <list>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

namespace yuan {

class Logger;

class LogEvent {
public:
    using ptr = std::shared_ptr<LogEvent>;
    LogEvent(const char *file, int32_t line, uint32_t threadId, uint32_t fiberId
        , uint64_t time, uint32_t elapse);

    const char *getFile() { return m_file; }
    int32_t getLine() { return m_line; }
    uint32_t getThreadId() { return m_threadId; }
    uint32_t getFiberId() { return m_fiberId; }
    uint64_t getTime() { return m_time; }
    uint32_t getElapse() { return m_elapse; }
    std::string getContent() { return m_ss.str(); }

    std::stringstream &getSS() { return m_ss; }
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
    std::stringstream m_ss;
};

class LogLevel {
public:
    enum Level {
        UNKNOWN = 0,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    static const char *ToString(Level level);
};

// 日志格式器
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    explicit LogFormatter(const std::string &pattern);

    // 固定化格式输出
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
public:
    // 日志解析的子模块, 会根据不同日志输出格式来实现子类：https://blog.csdn.net/ctwy291314/article/details/83822254
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        // os通常传stringstream
        virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };

private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
    void init();
};

// 日志输出地
class LogAppender {
public:
    typedef std::shared_ptr<LogAppender> ptr;
    virtual ~LogAppender() {}

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    void setFormatter(LogFormatter::ptr formatter) { m_formatter = formatter; }
    LogFormatter::ptr getFormatter() { return m_formatter; }
protected:
    LogLevel::Level m_level;
    LogFormatter::ptr m_formatter;
};

// 日志器。只有继承了enable_shared_from_this，才能在成员函数中获得指向自己的shared_ptr
// enable_shared_from_this: https://blog.csdn.net/caoshangpa/article/details/79392878
class Logger : public std::enable_shared_from_this<Logger> {
public:
    typedef std::shared_ptr<Logger> ptr;

    explicit Logger(const std::string &name = "root");
    void log(LogLevel::Level level, LogEvent::ptr event);

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    const std::string &getName() { return m_name; }
    LogLevel::Level getLevel() const { return m_level; }
    void setLevel(LogLevel::Level level) { m_level = level; }
    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
private:
    // 日志器名称
    std::string m_name;
    // 满足该日志级别才会输出
    LogLevel::Level m_level;
    // Appender集合
    std::list<LogAppender::ptr> m_appenders;
    // 可能有的LogAppender自己不提供formatter，因此就用Logger的
    LogFormatter::ptr m_formatter;
};

// 日志输出到控制台的Appender
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
private:
};

// 日志输出到文件的Appender 
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    explicit FileLogAppender(const std::string &filename);

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;

    // 重新打开文件，打开成功则返回true
    bool reopen();
private:
    std::string m_filename;
    std::ofstream m_filestream;
};

}





#endif