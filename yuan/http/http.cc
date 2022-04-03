#include "http.h"

namespace yuan {
namespace http {

/**
 * 以下为辅助工具函数
 */
HttpMethod StringToHttpMethod(const std::string &m) {
// 下面这种方式有大量的比较，以后尝试优化
#define XX(num, name, desc) \
    if (strcmp(#desc, m.c_str()) == 0) { \
        return HttpMethod::name; \
    }
    HTTP_METHOD_MAP(XX);
#undef XX
    return HttpMethod::INVALID_METHOD;
}

// 虽然前面已经有了string的转换，但后面总会调用一些系统的或第三方的方法得到char *的字符串。防止复制成string增加成本。专门写个方法。
HttpMethod CharsToHttpMethod(const char *m, size_t len) {
#define XX(num, name, desc) \
    if (strncmp(#desc, m, len) == 0) { \
        return HttpMethod::name; \
    }
    HTTP_METHOD_MAP(XX);
#undef XX
    return HttpMethod::INVALID_METHOD;
}

// 巧妙的利用数组，让下面的方法不需要大量判断
static const char * s_method_string[] = {
#define XX(num, name, desc) #desc,
    HTTP_METHOD_MAP(XX)
#undef XX
    "INVALID_METHOD"
};

const char *HttpMethodToString(HttpMethod hm) {
    uint32_t idx = static_cast<uint32_t>(hm);
    if (idx >= sizeof(s_method_string) / sizeof(s_method_string[0])) {
        return "INVALID_METHOD";
    }
    return s_method_string[idx];
}

const char *HttpStatusToString(HttpStatus hm) {
    switch (hm) {
#define XX(code, name, desc) \
    case HttpStatus::name: \
        return #desc;
    HTTP_STATUS_MAP(XX);
#undef XX
        default:
            return "<unknown>";
    }
}

bool CaseInsensitive::operator()(const std::string &lhs, const std::string &rhs) const {
    // 这种较复杂的操作，C语言提供的接口更多
    return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
}

/**
 * 以下为HttpRequest的函数实现
 */
HttpRequest::HttpRequest(uint8_t version, bool close)
    : m_method(HttpMethod::GET)
    , m_version(version)
    , m_close(close)
// 细节：需要给默认值的都要考虑到
    , m_path("/") {}

const std::string HttpRequest::getHeader(const std::string &key, const std::string &def) const {
    auto it = m_headers.find(key);
    return it == m_headers.end() ? def : it->second;
}

const std::string HttpRequest::getParam(const std::string &key, const std::string &def) const {
    auto it = m_params.find(key);
    return it == m_params.end() ? def : it->second;
}

const std::string HttpRequest::getCookie(const std::string &key, const std::string &def) const {
    auto it = m_cookies.find(key);
    return it == m_cookies.end() ? def : it->second;
}

bool HttpRequest::hasHeader(const std::string &key, std::string *val) const {
    auto it = m_headers.find(key);
    if (it == m_headers.end()) {
        return false;
    }

    if (val) {
        *val = it->second;
    }
    return true;
}

bool HttpRequest::hasParam(const std::string &key, std::string *val) const {
    auto it = m_params.find(key);
    if (it == m_params.end()) {
        return false;
    }

    if (val) {
        *val = it->second;
    }
    return true;
}

bool HttpRequest::hasCookie(const std::string &key, std::string *val) const {
    auto it = m_cookies.find(key);
    if (it == m_cookies.end()) {
        return false;
    }

    if (val) {
        *val = it->second;
    }
    return true;
}

void HttpRequest::setHeader(const std::string &key, const std::string &val) {
    m_headers[key] = val;
}

void HttpRequest::setParam(const std::string &key, const std::string &val) {
    m_params[key] = val;
}

void HttpRequest::setCookie(const std::string &key, const std::string &val) {
    m_cookies[key] = val;
}

void HttpRequest::delHeader(const std::string &key) {
    m_headers.erase(key);
}

void HttpRequest::delParam(const std::string &key) {
    m_params.erase(key);
}

void HttpRequest::delCookie(const std::string &key) {
    m_cookies.erase(key);
}

std::ostream &HttpRequest::dump(std::ostream &os) const {
    os << HttpMethodToString(m_method) << ' '
        << m_path
        << (m_query.empty() ? "" : "?")
        << m_query
        << (m_fragment.empty() ? "" : "#")
        << m_fragment
        << " HTTP/"
        << static_cast<int>(m_version >> 4)
        << "."
        << static_cast<int>(m_version & 0x0f)
        << "\r\n";

    os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    for (const auto &header : m_headers) {
        if (strcasecmp(header.first.c_str(), "connection") == 0) {
            continue;
        }
        os << header.first << ": " << header.second << "\r\n";
    }

    if (m_body.empty()) {
        os << "\r\n";
    } else {
        // 注意这里是字节长度，不是字符长度。string.size()获取的是字节长度，如utf8下一个汉字是3字节
        os << "content-length:" << m_body.size() << "\r\n\r\n" << m_body;
    }

    return os;
}

std::string HttpRequest::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}
/**
 * 以下为HttpResponse的函数实现
 */
HttpResponse::HttpResponse(uint8_t version, bool close)
// status默认给了200
    : m_status(HttpStatus::OK)
    , m_version(version)
    , m_close(close) {}

const std::string HttpResponse::getHeader(const std::string &key, const std::string &def) const {
    auto it = m_headers.find(key);
    return it == m_headers.end() ? def : it->second;
}

bool HttpResponse::hasHeader(const std::string &key, std::string *val) const {
    auto it = m_headers.find(key);
    if (it == m_headers.end()) {
        return false;
    }

    if (val) {
        *val = it->second;
    }
    return true;
}

void HttpResponse::setHeader(const std::string &key, const std::string &val) {
    m_headers[key] = val;
}

void HttpResponse::delHeader(const std::string &key) {
    m_headers.erase(key);
}

std::ostream &HttpResponse::dump(std::ostream &os) const {
    os << "HTTP/" << static_cast<uint32_t>(m_version >> 4)
        << "." << static_cast<uint32_t>(m_version & 0x0F) << " "
        << static_cast<uint32_t>(m_status) << " "
// 注意：填什么reason就返回什么reason，否则就按照标准
        << (m_reason.empty() ? HttpStatusToString(m_status) : m_reason)
        << "\r\n";

    os << "connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    for (auto &header : m_headers) {
        if (strcasecmp("connection", header.first.c_str()) == 0) {
            continue;
        }
        os << header.first << ": " << header.second << "\r\n";
    } 

    if (m_body.empty()) {
        os << "\r\n";
    } else {
        os << "content-length:" << m_body.size() << "\r\n\r\n"
            << m_body;
    }

    return os;
}

std::string HttpResponse::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

}
}