#include <iostream>
#include "../yuan/log.h"
#include "../yuan/util.h"

int main(int argc, char **argv) {
    yuan::Logger::ptr logger(new yuan::Logger());
    logger->addAppender(yuan::LogAppender::ptr(new yuan::StdoutLogAppender()));

    yuan::LogEvent::ptr event(new yuan::LogEvent(__FILE__, __LINE__
        , yuan::GetThreadId(), yuan::GetFiberId(), time(nullptr), 0));
    logger->log(yuan::LogLevel::INFO, event);
    return 0;
}