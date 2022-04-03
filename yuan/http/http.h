#ifndef __YUAN_HTTP_H__
#define __YUAN_HTTP_H__
/**
 * 查看服务器的状态，常常通过Http协议，可以查到某一台服务器的状态
 * 主要封装出来HttpRequest和HttpResponse两个结构体
 */
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace yuan {
namespace http {

/* Request Methods */
#define HTTP_METHOD_MAP(XX)         \
  XX(0,  DELETE,      DELETE)       \
  XX(1,  GET,         GET)          \
  XX(2,  HEAD,        HEAD)         \
  XX(3,  POST,        POST)         \
  XX(4,  PUT,         PUT)          \
  /* pathological */                \
  XX(5,  CONNECT,     CONNECT)      \
  XX(6,  OPTIONS,     OPTIONS)      \
  XX(7,  TRACE,       TRACE)        \
  /* WebDAV */                      \
  XX(8,  COPY,        COPY)         \
  XX(9,  LOCK,        LOCK)         \
  XX(10, MKCOL,       MKCOL)        \
  XX(11, MOVE,        MOVE)         \
  XX(12, PROPFIND,    PROPFIND)     \
  XX(13, PROPPATCH,   PROPPATCH)    \
  XX(14, SEARCH,      SEARCH)       \
  XX(15, UNLOCK,      UNLOCK)       \
  XX(16, BIND,        BIND)         \
  XX(17, REBIND,      REBIND)       \
  XX(18, UNBIND,      UNBIND)       \
  XX(19, ACL,         ACL)          \
  /* subversion */                  \
  XX(20, REPORT,      REPORT)       \
  XX(21, MKACTIVITY,  MKACTIVITY)   \
  XX(22, CHECKOUT,    CHECKOUT)     \
  XX(23, MERGE,       MERGE)        \
  /* upnp */                        \
  XX(24, MSEARCH,     M-SEARCH)     \
  XX(25, NOTIFY,      NOTIFY)       \
  XX(26, SUBSCRIBE,   SUBSCRIBE)    \
  XX(27, UNSUBSCRIBE, UNSUBSCRIBE)  \
  /* RFC-5789 */                    \
  XX(28, PATCH,       PATCH)        \
  XX(29, PURGE,       PURGE)        \
  /* CalDAV */                      \
  XX(30, MKCALENDAR,  MKCALENDAR)   \
  /* RFC-2068, section 19.6.1.2 */  \
  XX(31, LINK,        LINK)         \
  XX(32, UNLINK,      UNLINK)       \
  /* icecast */                     \
  XX(33, SOURCE,      SOURCE)       \

/* Status Codes */
#define HTTP_STATUS_MAP(XX)                                                 \
  XX(100, CONTINUE,                        Continue)                        \
  XX(101, SWITCHING_PROTOCOLS,             Switching Protocols)             \
  XX(102, PROCESSING,                      Processing)                      \
  XX(200, OK,                              OK)                              \
  XX(201, CREATED,                         Created)                         \
  XX(202, ACCEPTED,                        Accepted)                        \
  XX(203, NON_AUTHORITATIVE_INFORMATION,   Non-Authoritative Information)   \
  XX(204, NO_CONTENT,                      No Content)                      \
  XX(205, RESET_CONTENT,                   Reset Content)                   \
  XX(206, PARTIAL_CONTENT,                 Partial Content)                 \
  XX(207, MULTI_STATUS,                    Multi-Status)                    \
  XX(208, ALREADY_REPORTED,                Already Reported)                \
  XX(226, IM_USED,                         IM Used)                         \
  XX(300, MULTIPLE_CHOICES,                Multiple Choices)                \
  XX(301, MOVED_PERMANENTLY,               Moved Permanently)               \
  XX(302, FOUND,                           Found)                           \
  XX(303, SEE_OTHER,                       See Other)                       \
  XX(304, NOT_MODIFIED,                    Not Modified)                    \
  XX(305, USE_PROXY,                       Use Proxy)                       \
  XX(307, TEMPORARY_REDIRECT,              Temporary Redirect)              \
  XX(308, PERMANENT_REDIRECT,              Permanent Redirect)              \
  XX(400, BAD_REQUEST,                     Bad Request)                     \
  XX(401, UNAUTHORIZED,                    Unauthorized)                    \
  XX(402, PAYMENT_REQUIRED,                Payment Required)                \
  XX(403, FORBIDDEN,                       Forbidden)                       \
  XX(404, NOT_FOUND,                       Not Found)                       \
  XX(405, METHOD_NOT_ALLOWED,              Method Not Allowed)              \
  XX(406, NOT_ACCEPTABLE,                  Not Acceptable)                  \
  XX(407, PROXY_AUTHENTICATION_REQUIRED,   Proxy Authentication Required)   \
  XX(408, REQUEST_TIMEOUT,                 Request Timeout)                 \
  XX(409, CONFLICT,                        Conflict)                        \
  XX(410, GONE,                            Gone)                            \
  XX(411, LENGTH_REQUIRED,                 Length Required)                 \
  XX(412, PRECONDITION_FAILED,             Precondition Failed)             \
  XX(413, PAYLOAD_TOO_LARGE,               Payload Too Large)               \
  XX(414, URI_TOO_LONG,                    URI Too Long)                    \
  XX(415, UNSUPPORTED_MEDIA_TYPE,          Unsupported Media Type)          \
  XX(416, RANGE_NOT_SATISFIABLE,           Range Not Satisfiable)           \
  XX(417, EXPECTATION_FAILED,              Expectation Failed)              \
  XX(421, MISDIRECTED_REQUEST,             Misdirected Request)             \
  XX(422, UNPROCESSABLE_ENTITY,            Unprocessable Entity)            \
  XX(423, LOCKED,                          Locked)                          \
  XX(424, FAILED_DEPENDENCY,               Failed Dependency)               \
  XX(426, UPGRADE_REQUIRED,                Upgrade Required)                \
  XX(428, PRECONDITION_REQUIRED,           Precondition Required)           \
  XX(429, TOO_MANY_REQUESTS,               Too Many Requests)               \
  XX(431, REQUEST_HEADER_FIELDS_TOO_LARGE, Request Header Fields Too Large) \
  XX(451, UNAVAILABLE_FOR_LEGAL_REASONS,   Unavailable For Legal Reasons)   \
  XX(500, INTERNAL_SERVER_ERROR,           Internal Server Error)           \
  XX(501, NOT_IMPLEMENTED,                 Not Implemented)                 \
  XX(502, BAD_GATEWAY,                     Bad Gateway)                     \
  XX(503, SERVICE_UNAVAILABLE,             Service Unavailable)             \
  XX(504, GATEWAY_TIMEOUT,                 Gateway Timeout)                 \
  XX(505, HTTP_VERSION_NOT_SUPPORTED,      HTTP Version Not Supported)      \
  XX(506, VARIANT_ALSO_NEGOTIATES,         Variant Also Negotiates)         \
  XX(507, INSUFFICIENT_STORAGE,            Insufficient Storage)            \
  XX(508, LOOP_DETECTED,                   Loop Detected)                   \
  XX(510, NOT_EXTENDED,                    Not Extended)                    \
  XX(511, NETWORK_AUTHENTICATION_REQUIRED, Network Authentication Required) \

// 细节：用class，scoped enum，命名不会发生冲突。也方便下面各种转换时方便定义宏
enum class HttpMethod {
#define XX(num, name, desc) name = num,
    HTTP_METHOD_MAP(XX)
#undef XX
    INVALID_METHOD
};

enum class HttpStatus {
#define XX(code, name, desc) name = code,
    HTTP_STATUS_MAP(XX)
#undef XX
};

HttpMethod StringToHttpMethod(const std::string &m);
HttpMethod CharsToHttpMethod(const char *m, size_t len);
const char *HttpMethodToString(HttpMethod hm);
const char *HttpStatusToString(HttpStatus hm);

struct CaseInsensitive {
  bool operator()(const std::string &lhs, const std::string &rhs) const;
};

// 通用重点方法：找到对应的值字符串，并做相应的类型转换
template<typename MapType, typename T>
bool checkGetAs(const MapType &m, const std::string &key, T &val, const T &def = T()) {
  auto it = m.find(key);
  if (it == m.end()) {
    val = def;
    return false;
  }

  try {
    val = boost::lexical_cast<T>(it->second);
    return true;
  } catch (...) {
    val = def;
  }
  return false;
}

  // 和checkGetAs区别在于不返回是否存在，直接返回值
template<typename MapType, typename T>
const T getAs(const MapType &m, const std::string &key, const T &def) {
  auto it = m.find(key);
  if (it == m.end()) {
    return def;
  }

  try {
    return boost::lexical_cast<T>(it->second);
  } catch(...) {
  }
  return def;
}
/**
 * HTTP请求的封装
 */
class HttpRequest {
public:
  typedef std::shared_ptr<HttpRequest> ptr;
  // 技巧：避免总是写太长
  typedef std::map<std::string, std::string, CaseInsensitive> MapType;

  HttpRequest(uint8_t version = 0x11, bool close = true);

  HttpMethod getMethod() const { return m_method; }
  uint8_t getVersion() const { return m_version; }
  bool isClose() const { return m_close; }
  const std::string &getPath() const { return m_path; }
  const std::string &getQuery() const { return m_query; }
  const std::string &getFragment() const { return m_fragment; }
  const MapType &getHeaders() const { return m_headers; }
  const MapType &getParams() const { return m_params; }
  const MapType &getCookies() const { return m_cookies; }

  void setMethod(HttpMethod v) { m_method = v; }
  void setVersion(uint8_t v) { m_version = v; }
  void setClose(bool v) { m_close = v; }

  void setPath(const std::string &v) { m_path = v; }
  void setQuery(const std::string &v) { m_query = v; }
  void setFragment(const std::string &v) { m_fragment = v; }
  void setBody(const std::string &v) { m_body = v; }

  void setHeaders(const MapType &v) { m_headers = v; }
  void setParams(const MapType &v) { m_params = v; }
  void setCookies(const MapType &v) { m_cookies = v; }

  const std::string getHeader(const std::string &key, const std::string &def = "") const;
  const std::string getParam(const std::string &key, const std::string &def = "") const;
  const std::string getCookie(const std::string &key, const std::string &def = "") const;

  bool hasHeader(const std::string &key, std::string *val = nullptr) const;
  bool hasParam(const std::string &key, std::string *val = nullptr) const;
  bool hasCookie(const std::string &key, std::string *val = nullptr) const;

  void setHeader(const std::string &key, const std::string &val);
  void setParam(const std::string &key, const std::string &val);
  void setCookie(const std::string &key, const std::string &val);

  void delHeader(const std::string &key);
  void delParam(const std::string &key);
  void delCookie(const std::string &key);

  // 注意模板方法必须在头文件里定义
  template<typename T>
  bool checkGetHeaderAs(const std::string &key, T &val, const T &def = T()) {
    return checkGetAs(m_headers, key, val, def);
  }

  template<typename T>
  const T getHeaderAs(const std::string &key, const T &def = T()) {
    return getAs(m_headers, key, def);
  }

  template<typename T>
  bool checkGetParamsAs(const std::string &key, T &val, const T &def = T()) {
    return checkGetAs(m_params, key, val, def);
  }

  template<typename T>
  const T getParamsAs(const std::string &key, const T &def = T()) {
    return getAs(m_params, key, def);
  }

  template<typename T>
  bool checkGetCookiesAs(const std::string &key, T &val, const T &def = T()) {
    return checkGetAs(m_cookies, key, val, def);
  }

  template<typename T>
  const T getCookiesAs(const std::string &key, const T &def = T()) {
    return getAs(m_cookies, key, def);
  }

  // 重点方法：比如在发送Http请求的时候，还是要封装成文本格式发出去
  std::ostream &dump(std::ostream &os) const;
  std::string toString() const;

private:
  HttpMethod m_method;
  // 技巧：Http版本，比如1.1，可以用0x11表示
  uint8_t m_version;
  // 从1.1开始支持长连接
  bool m_close;

  // URI里的文件路径
  std::string m_path;
  // URI里的请求参数
  std::string m_query;
  // URI里的fragment，https://www.jianshu.com/p/2c07fbb52b45
  std::string m_fragment;
  // 请求体
  std::string m_body;

  // 请求头。重点：因为不区分大小写，因此要修改map中的比较函数。这就是不用unordered_map的原因，既需要重新设计Hash函数，也需要设置比较函数
  MapType m_headers;
  // 参数：可能有两个来源：url上拼接的和post等方法里请求体中解析出来的
  MapType m_params;
  MapType m_cookies;

};

/**
 * HTTP响应的封装。要比HttpRequest的结构简单一些，因为是业务方在解析它，而HttpResonse是后端在解析
 */
class HttpResponse {
public:
  typedef std::shared_ptr<HttpResponse> ptr;
  typedef std::map<std::string, std::string, CaseInsensitive> MapType;

  HttpResponse(uint8_t version = 0x11, bool close = true);

  HttpStatus getStatus() const { return m_status; }
  uint8_t getVersion() const { return m_version; }
  const std::string &getReason() const { return m_reason; }
  bool isClose() const { return m_close; }
  const std::string &getBody() const { return m_body; }
  const MapType &getHeaders() const { return m_headers; }

  void setStatus(HttpStatus v) { m_status = v; }
  void setReason(const std::string &v) { m_reason = v; }
  void setVersion(uint8_t v) { m_version = v; }
  void setBody(const std::string &v) { m_body = v; }
  void setHeaders(const MapType &v) { m_headers = v; }
  void setClose(bool v) { m_close = v; }

  const std::string getHeader(const std::string &key, const std::string &def = "") const;
  bool hasHeader(const std::string &key, std::string *val = nullptr) const;
  void setHeader(const std::string &key, const std::string &val);
  void delHeader(const std::string &key);

  std::ostream &dump(std::ostream &os) const;
  std::string toString() const;

private:
  HttpStatus m_status;
  std::string m_reason;
  uint8_t m_version;
  bool m_close;

  std::string m_body;

  MapType m_headers;
};

}
}

#endif