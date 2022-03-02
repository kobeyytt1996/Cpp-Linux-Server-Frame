#include <iostream>
#include "../yuan/log.h"
#include "../yuan/util.h"

int main(int argc, char **argv) {
    yuan::Logger::ptr logger(new yuan::Logger());
    logger->addAppender(yuan::LogAppender::ptr(new yuan::StdoutLogAppender()));

    // yuan::LogEvent::ptr event(new yuan::LogEvent(logger, yuan::LogLevel::DEBUG, __FILE__, __LINE__
    //     , yuan::GetThreadId(), yuan::GetFiberId(), time(nullptr), 0));
    // logger->log(yuan::LogLevel::INFO, event);
    YUAN_LOG_DEBUG(logger) << "test macro";
    return 0;
}