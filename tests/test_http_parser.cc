#include "../yuan/http/http_parser.h"
#include "../yuan/log.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

static char g_test_request_data[] = "POST / HTTP/1.1\r\n"
                                    "Content-Length: 10\r\n"
                                    "Host: www.baidu.com\r\n\r\n"
                                    "1234567890";

static char g_test_response_data[] = "HTTP/1.1 200 OK\r\n"
        "Date: Tue, 04 Jun 2019 15:43:56 GMT\r\n"
        "Server: Apache\r\n"
        "Last-Modified: Tue, 12 Jan 2010 13:48:00 GMT\r\n"
        "ETag: \"51-47cf7e6ee8400\"\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 81\r\n"
        "Cache-Control: max-age=86400\r\n"
        "Expires: Wed, 05 Jun 2019 15:43:56 GMT\r\n"
        "Connection: Close\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<html>\r\n"
        "<meta http-equiv=\"refresh\" content=\"0;url=http://www.baidu.com/\">\r\n"
        "</html>\r\n";

void test_request() {
    yuan::http::HttpRequestParser::ptr parser(std::make_shared<yuan::http::HttpRequestParser>());
    size_t ret = parser->execute(g_test_request_data, sizeof(g_test_request_data));
    YUAN_LOG_INFO(g_logger) << "execute ret=" << ret << " has_error=" << parser->hasError()
        << " is_finished=" << parser->isFinished()
        << " total_chars=" << sizeof(g_test_request_data)
        << " Content-Length=" << parser->getContentLength();

    YUAN_LOG_INFO(g_logger) << parser->getData()->toString();
    YUAN_LOG_INFO(g_logger) << g_test_request_data;
}

void test_response() {
    yuan::http::HttpResponseParser::ptr parser(std::make_shared<yuan::http::HttpResponseParser>());
    size_t ret = parser->execute(g_test_response_data, sizeof(g_test_response_data), false);
    YUAN_LOG_INFO(g_logger) << "execute ret=" << ret << " has_error=" << parser->hasError()
        << " is_finished=" << parser->isFinished()
        << " total_chars=" << sizeof(g_test_response_data)
        << " Content-Length=" << parser->getContentLength();

    YUAN_LOG_INFO(g_logger) << parser->getData()->toString();
    YUAN_LOG_INFO(g_logger) << g_test_response_data;
}

int main(int argc, char **argv) {
    test_request();
    test_response();
    return 0;
}