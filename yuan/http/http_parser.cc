#include "../config.h"
#include "http_parser.h"
#include "../log.h"
#include <string.h>

namespace yuan {
namespace http {

static yuan::Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");
/**
 * 以下是约定了一些可以修改的配置项
 */
// 虽然Http没有规定每个字段有多长，但是要防范有人恶意发包。比如规定头部长度不能超过4k，超过即认为非法。对于服务器，外部的请求方都是危险的，一定要防范
static yuan::ConfigVar<uint64_t>::ptr g_http_request_buffer_size = 
    yuan::Config::Lookup("http.request.buffer_size", (uint64_t)4 * 1024, "http request buffer size");
// 同样，对于请求体也有相同的约定.默认最大为64m
static yuan::ConfigVar<uint64_t>::ptr g_http_request_max_body_size = 
    yuan::Config::Lookup("http.request.max_body_size", (uint64_t)64 * 1024 * 1024, "http request max body size");

// 常用方式。因为约定项的getValue里有加锁。为了性能，用变量记录值，并增加回调，有修改则相应的改变值
static uint64_t s_http_request_buffer_size = 0;
static uint64_t s_http_request_max_body_size = 0;

// 只是为了初始化的类和对象，放在匿名namespace里，防止污染命名空间
namespace {
struct _RequestSizeIniter {
    // 全局变量的初始化在main函数之前
    _RequestSizeIniter() {
        s_http_request_buffer_size = g_http_request_buffer_size->getValue();
        g_http_request_buffer_size->add_listener([](const uint64_t &old_val, const uint64_t &new_val){
            s_http_request_buffer_size = new_val;
        });
        s_http_request_max_body_size = g_http_request_max_body_size->getValue();
        g_http_request_max_body_size->add_listener([](const uint64_t &old_val, const uint64_t &new_val){
            s_http_request_max_body_size = new_val;
        });
    }
};
// 全局变量的初始化在main函数之前
static _RequestSizeIniter _init;
}

uint64_t HttpRequestParser::GetHttpRequestBufferSize() {
    return s_http_request_buffer_size;
}

uint64_t HttpRequestParser::GetHttpRequestMaxBodySize() {
    return s_http_request_max_body_size;
}
/**
 * request解析器里各种回调函数的定义
 */
void on_request_method(void *data, const char *at, size_t length) {
    HttpRequestParser *parser = static_cast<HttpRequestParser*>(data);
    HttpMethod method = CharsToHttpMethod(at, length);

    if (method == HttpMethod::INVALID_METHOD) {
        YUAN_LOG_WARN(g_system_logger) << "invalid http request method:" << std::string(at, length);
        parser->setError(1000);
        return;
    }
    parser->getData()->setMethod(method);
}

void on_request_uri(void *data, const char *at, size_t length) {
    
}

void on_request_fragment(void *data, const char *at, size_t length) {
    HttpRequestParser *parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setFragment(std::string(at, length));
}

void on_request_path(void *data, const char *at, size_t length) {
    HttpRequestParser *parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setPath(std::string(at, length));
}

void on_request_query(void *data, const char *at, size_t length) {
    HttpRequestParser *parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setQuery(std::string(at, length));
}

void on_request_version(void *data, const char *at, size_t length) {
    HttpRequestParser *parser = static_cast<HttpRequestParser*>(data);
    uint8_t version = 0;
    if (strncasecmp("HTTP/1.0", at, length) == 0) {
        version = 0x10;
    } else if (strncasecmp("HTTP/1.1", at, length) == 0) {
        version = 0x11;
    } else {
        YUAN_LOG_WARN(g_system_logger) << "invalid http request version: " << std::string(at, length);
        parser->setError(1001);
        return;
    }
    parser->getData()->setVersion(version);
}

void on_request_header_done(void *data, const char *at, size_t length) {

}

// 解析出了头部的信息，即key，value
void on_request_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen) {
    HttpRequestParser *parser = static_cast<HttpRequestParser*>(data);
    if (flen == 0) {
        YUAN_LOG_WARN(g_system_logger) << "invalid http request field length == 0";
        parser->setError(1002);
        return;
    }
    parser->getData()->setHeader(std::string(field, flen), std::string(value, vlen));
}

/**
 * HttpRequestParser方法的实现
 */
HttpRequestParser::HttpRequestParser()
    : m_data(std::make_shared<HttpRequest>())
    , m_error(0) {
    http_parser_init(&m_parser);
    // 以下是设置parser中的回调函数
    m_parser.request_method = on_request_method;
    m_parser.request_uri = on_request_uri;
    m_parser.fragment = on_request_fragment;
    m_parser.request_path = on_request_path;
    m_parser.query_string = on_request_query;
    m_parser.http_version = on_request_version;
    m_parser.header_done = on_request_header_done;
    m_parser.http_field = on_request_http_field;
    // 这样上述回调被调用时才能获取到HttpRequestParser对象
    m_parser.data = this;
}

size_t HttpRequestParser::execute(char *data, size_t len) {
    size_t parsed_len = http_parser_execute(&m_parser, data, len, 0);
    // 上面返回的是已解析的长度。因为data的信息可能一部分和下一次从缓冲区读出来的是一个整体，所以没解析的这部分数据也要留着，更新data
    memmove(data, data + parsed_len, len - parsed_len);
    return parsed_len;
}

int HttpRequestParser::isFinished() {
    return http_parser_finish(&m_parser);
}

int HttpRequestParser::hasError() {
    return m_error || http_parser_has_error(&m_parser);
}

uint64_t HttpRequestParser::getContentLength() {
    return m_data->getHeaderAs<uint64_t>("Content-Length", 0);
}
/**
 * request解析器里各种回调函数的定义
 */
void on_response_reason(void *data, const char *at, size_t length) {
    HttpResponseParser *parser = static_cast<HttpResponseParser*>(data);
    parser->getData()->setReason(std::string(at, length));
}

void on_response_status(void *data, const char *at, size_t length) {
    HttpResponseParser *parser = static_cast<HttpResponseParser*>(data);
    HttpStatus status = static_cast<HttpStatus>(atoi(at));
    parser->getData()->setStatus(status);
}

void on_response_chunk(void *data, const char *at, size_t length) {

}

void on_response_version(void *data, const char *at, size_t length) {
    HttpResponseParser *parser = static_cast<HttpResponseParser*>(data);
    if (strncasecmp("HTTP/1.0", at, length) == 0) {
        parser->getData()->setVersion(0x10);
    } else if (strncasecmp("HTTP/1.1", at, length) == 0) {
        parser->getData()->setVersion(0x11);
    } else {
        YUAN_LOG_WARN(g_system_logger) << "invalid http response version: " << std::string(at, length);
        parser->setError(1001);
        return;
    }

}

void on_response_header_done(void *data, const char *at, size_t length) {

}

void on_response_last_chunk(void *data, const char *at, size_t length) {

}

void on_response_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen) {
    HttpResponseParser *parser = static_cast<HttpResponseParser*>(data);
    if (flen == 0) {
        YUAN_LOG_WARN(g_system_logger) << "invalid http response field length == 0";
        parser->setError(1002);
        return;
    }
    parser->getData()->setHeader(std::string(field, flen), std::string(value, vlen));
}

HttpResponseParser::HttpResponseParser()
    : m_data(std::make_shared<HttpResponse>())
    , m_error(0) {
    httpclient_parser_init(&m_parser);
    m_parser.reason_phrase = on_response_reason;
    m_parser.status_code = on_response_status;
    m_parser.chunk_size = on_response_chunk;
    m_parser.http_version = on_response_version;
    m_parser.header_done = on_response_header_done;
    m_parser.last_chunk = on_response_last_chunk;
    m_parser.http_field = on_response_http_field;

    m_parser.data = this;
}

size_t HttpResponseParser::execute(char *data, size_t len) {
    size_t parsed_len = httpclient_parser_execute(&m_parser, data, len, 0);
    memmove(data, data + parsed_len, len - parsed_len);
    return parsed_len;
}

int HttpResponseParser::isFinished() {
    return httpclient_parser_finish(&m_parser);
}

int HttpResponseParser::hasError() {
    return m_error || httpclient_parser_has_error(&m_parser);
}

uint64_t HttpResponseParser::getContentLength() {
    return m_data->getHeaderAs<uint64_t>("Content-Length", 0);
}

}
}