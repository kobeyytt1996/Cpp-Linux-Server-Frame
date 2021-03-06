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
#include <stdarg.h>
#include <map>
#include "singleton.h"
#include "util.h"
#include "thread.h"

// 定义一些宏让日志系统更好用。注意宏里的namespace千万别忽略
// 返回stringstream更方便使用者流式调用并增加自己的输出内容
//  注意:__FILE__原本是绝对路径，但在CMakeLists.txt里配置为了相对路径。
#define YUAN_LOG_LEVEL(logger, level) \
    if(level >= logger->getLevel()) \
        yuan::LogEventWrap(yuan::LogEvent::ptr(new yuan::LogEvent(logger, level, __FILE__, __LINE__ \
        , yuan::GetThreadId(), yuan::GetFiberId(), time(nullptr), 0, yuan::Thread::GetName()))).getSS()

#define YUAN_LOG_DEBUG(logger) YUAN_LOG_LEVEL(logger, yuan::LogLevel::DEBUG)
#define YUAN_LOG_INFO(logger) YUAN_LOG_LEVEL(logger, yuan::LogLevel::INFO)
#define YUAN_LOG_WARN(logger) YUAN_LOG_LEVEL(logger, yuan::LogLevel::WARN)
#define YUAN_LOG_ERROR(logger) YUAN_LOG_LEVEL(logger, yuan::LogLevel::ERROR)
#define YUAN_LOG_FATAL(logger) YUAN_LOG_LEVEL(logger, yuan::LogLevel::FATAL)

// 让用户可以像printf一样调用，再原来的日志后增加自定义内容
#define YUAN_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(level >= logger->getLevel()) \
        yuan::LogEventWrap(yuan::LogEvent::ptr(new yuan::LogEvent(logger, level, __FILE__, __LINE__ \
        , yuan::GetThreadId(), yuan::GetFiberId(), time(nullptr), 0, yuan::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)

#define YUAN_LOG_FMT_DEBUG(logger, fmt, ...) YUAN_LOG_FMT_LEVEL(logger, yuan::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define YUAN_LOG_FMT_INFO(logger, fmt, ...) YUAN_LOG_FMT_LEVEL(logger, yuan::LogLevel::INFO, fmt, __VA_ARGS__)
#define YUAN_LOG_FMT_WARN(logger, fmt, ...) YUAN_LOG_FMT_LEVEL(logger, yuan::LogLevel::WARN, fmt, __VA_ARGS__)
#define YUAN_LOG_FMT_ERROR(logger, fmt, ...) YUAN_LOG_FMT_LEVEL(logger, yuan::LogLevel::ERROR, fmt, __VA_ARGS__)
#define YUAN_LOG_FMT_FATAL(logger, fmt, ...) YUAN_LOG_FMT_LEVEL(logger, yuan::LogLevel::FATAL, fmt, __VA_ARGS__)

#define YUAN_GET_ROOT_LOGGER() yuan::LoggerMgr::GetInstance()->getRootLogger()
#define YUAN_GET_LOGGER(name) yuan::LoggerMgr::GetInstance()->getLogger(name)

namespace yuan {

class Logger;

//日志级别
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
    static Level FromString(const std::string &str);
};

class LogEvent {
public:
    using ptr = std::shared_ptr<LogEvent>;
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level
        , const char *file, int32_t line, uint32_t threadId, uint32_t fiberId
        , uint64_t time, uint32_t elapse, const std::string &thread_name);

    const char *getFile() { return m_file; }
    int32_t getLine() { return m_line; }
    uint32_t getThreadId() { return m_threadId; }
    const std::string getThreadName() { return m_threadName; }
    uint32_t getFiberId() { return m_fiberId; }
    uint64_t getTime() { return m_time; }
    uint32_t getElapse() { return m_elapse; }
    std::string getContent() { return m_ss.str(); }

    std::stringstream &getSS() { return m_ss; }
    std::shared_ptr<Logger> getLogger() { return m_logger; }
    LogLevel::Level getLevel() { return m_level; }

    // 这两个format方法是为了让用户可以像printf一样传format附加到原本的日志输出后
    void format(const char *fmt, ...);
    void format(const char *fmt, va_list al);
private:
    // 文件名
    const char * m_file = nullptr; 
    // 行号
    int32_t m_line = 0;
    uint32_t m_threadId = 0;
    std::string m_threadName;
    // 协程ID
    uint32_t m_fiberId = 0;
    // 时间戳
    uint64_t m_time = 0;
    // 程序启动到现在的毫秒数
    uint32_t m_elapse = 0;
    // 存储用户自己输入的数据，对应日志格式%m
    std::stringstream m_ss;

    std::shared_ptr<Logger> m_logger;
    LogLevel::Level m_level;
};

// 非常精妙的构思，为了让最上方输出日志的宏可以方便的流式补充日志输出内容,e.g. YUAN_LOG_DEBUG(logger) << "test macro";
// 增加一个包装类，利用临时对象当前行会被销毁的特性，在析构函数里调用日志输出
class LogEventWrap {
public:
    LogEventWrap(LogEvent::ptr event);
    ~LogEventWrap();

    std::stringstream &getSS() { return m_event->getSS(); }
    LogEvent::ptr getEvent() { return m_event; }
private:
    LogEvent::ptr m_event;
};

// 日志格式器。方法不需要加锁，因为没有改变其成员的方法
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

    bool isError() const { return m_error; }
    const std::string getPattern() const { return m_pattern; }
private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
    // 标记输入的格式字符串是否有模式错误
    bool m_error = false;
    void init();
};

// 日志输出地
class LogAppender {
friend class Logger;
public:
    typedef std::shared_ptr<LogAppender> ptr;
    // typedef的妙用：可以在这里方便的改变使用的锁的类型。（相当于把类型当变量）
    typedef Spinlock MutexType;
    virtual ~LogAppender() {}

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    void setLevel(LogLevel::Level level) { m_level = level; }
    void setFormatter(const std::string &format);
    void setFormatter(LogFormatter::ptr formatter);
    LogFormatter::ptr getFormatter();

    // 便于调试
    virtual std::string toYAMLString() = 0;
protected:
    // 一定要初始化，否则数值会是随机的
    LogLevel::Level m_level = LogLevel::DEBUG;
    // 复杂类型多线程使用时尤其需要加锁。上面level这种原子类型的线程不安全顶多是值不符合预期，
    // 但复杂类型由多个成员组成，修改值时不是原子操作，多线程下可能内存错误等崩溃，要避免的是这种线程安全问题
    LogFormatter::ptr m_formatter;
    // 标志自己是否有formatter，还是沿用Logger里的，只要被调用过setFormatter，就会设置为true
    bool m_hasFormatter = false;
    // 写比较多，故用普通锁即可
    MutexType m_mutex;
};

// 日志器。可以有多个logger，如系统和业务分离logger。用日志器名称来区分
// 只有继承了enable_shared_from_this，才能在成员函数中获得指向自己的shared_ptr。继承了该类，则不能在栈上生成该类的对象，无法用智能指针包裹
// enable_shared_from_this: https://blog.csdn.net/caoshangpa/article/details/79392878
class Logger : public std::enable_shared_from_this<Logger> {
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> ptr;
    typedef Spinlock MutexType;

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
    void clearAppenders();

    void setFormatter(const std::string &format);
    void setFormatter(LogFormatter::ptr formatter);
    LogFormatter::ptr getFormatter() const;
    // 便于调试
    std::string toYAMLString();
private:
    // 日志器名称
    std::string m_name;
    // 满足该日志级别才会输出
    LogLevel::Level m_level;
    // Appender集合
    std::list<LogAppender::ptr> m_appenders;
    // 可能有的LogAppender自己不提供formatter，因此就用Logger的
    LogFormatter::ptr m_formatter;
    // 如果用户并没有对该logger进行配置，则走默认root的日志行为
    Logger::ptr m_root;
    // 只要操作复杂对象，m_appenders和m_formatter，都要加锁，防止复杂类型的线程安全问题导致崩溃
    MutexType m_mutex;
};

// 日志输出到控制台的Appender
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYAMLString() override;
private:
};

// 日志输出到文件的Appender。后续：可以继承它来实现更多功能。比如一天一个配置文件，或者文件满300M了再写一个新的文件 
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    explicit FileLogAppender(const std::string &filename);
    ~FileLogAppender();

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;

    // 重新打开文件，打开成功则返回true
    bool reopen();
    // 便于调试
    std::string toYAMLString() override;
private:
    std::string m_filename;
    std::ofstream m_filestream;
    // 防止文件被删除，计时，每隔一段时间reopen一次
    uint64_t m_lastTime = 0;
};

// 管理所有logger，要用的时候直接从里面拿即可。单例类，使用时用下面LoggerMgr
class LoggerManager {
public:
    typedef Spinlock MutexType;

    LoggerManager();

    Logger::ptr getLogger(const std::string &name);
    Logger::ptr getRootLogger() const { return m_root; };

    void init();
    // 便于调试
    std::string toYAMLString();
private:
    std::map<std::string, Logger::ptr> m_loggers;
    // 默认Logger
    Logger::ptr m_root;
    MutexType m_mutex;
};

typedef SingletonPtr<LoggerManager> LoggerMgr;

}





#endif