#include <iostream>
#include "../yuan/log.h"
#include "../yuan/util.h"

int main(int argc, char **argv) {
    yuan::Logger::ptr logger(new yuan::Logger());
    logger->addAppender(yuan::LogAppender::ptr(new yuan::StdoutLogAppender()));

    yuan::FileLogAppender::ptr file_appender(new yuan::FileLogAppender("./log.txt"));
    logger->addAppender(file_appender);
    // 自定义输出pattern
    yuan::LogFormatter::ptr fmt(new yuan::LogFormatter("%d%T%p%T%m%n"));
    file_appender->setFormatter(fmt);
    // e.g. 文件里仅存error级别
    file_appender->setLevel(yuan::LogLevel::ERROR);

    // yuan::LogEvent::ptr event(new yuan::LogEvent(logger, yuan::LogLevel::DEBUG, __FILE__, __LINE__
    //     , yuan::GetThreadId(), yuan::GetFiberId(), time(nullptr), 0));
    // logger->log(yuan::LogLevel::INFO, event);
    // YUAN_LOG_DEBUG(logger) << "test macro";
    YUAN_LOG_FMT_ERROR(logger, "test error %s", "aa");

    auto l = yuan::LoggerMgr::GetInstance()->getLogger("xx");
    YUAN_LOG_INFO(l) << "xxx";
    return 0;
}