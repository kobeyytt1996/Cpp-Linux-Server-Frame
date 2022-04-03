#include "../yuan/http/http_parser.h"
#include "../yuan/log.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

static char g_test_request_data[] = "GET / HTTP/1.1\r\n"
                                    "Content-Length: 10\r\n"
                                    "Host: www.baidu.com\r\n\r\n"
                                    "1234567890";

void test() {
    yuan::http::HttpRequestParser::ptr parser(std::make_shared<yuan::http::HttpRequestParser>());
    size_t ret = parser->execute(g_test_request_data, sizeof(g_test_request_data));
    YUAN_LOG_INFO(g_logger) << "execute ret=" << ret << " has_error=" << parser->hasError()
        << " is_finished=" << parser->isFinished();

    YUAN_LOG_INFO(g_logger) << parser->getData()->toString();
}

int main(int argc, char **argv) {
    test();
    return 0;
}