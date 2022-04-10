#ifndef __YUAN_HTTP_Connection_H__
#define __YUAN_HTTP_Connection_H__
/**
 * 通常的命名规则：HttpSession指服务端accept请求之后的socket
 * HttpConnection指客户端要建立连接的socket
 */
#include "../socket_stream.h"
#include "http.h"
#include "../uri.h"

namespace yuan {
namespace http {

// 用一个结构体更方便的封装http请求后获得的结果
struct HttpResult {
    typedef std::shared_ptr<HttpResult> ptr;
    enum class Result {
        OK = 0,
        INVALID_URL = 1
    };
    HttpResult(Result _result, HttpResponse::ptr _response, const std::string &_error) 
        : result(_result)
        , response(_response)
        , error(_error) {}
    Result result;
    HttpResponse::ptr response;
    // 说明具体哪里出现错误
    std::string error;
};

class HttpConnection : public SocketStream {
public:
    typedef std::shared_ptr<HttpConnection> ptr;
    HttpConnection(Socket::ptr &sock, bool isOwner = true);

    // 核心方法：即根据Http协议特点解析http响应
    HttpResponse::ptr recvResponse();
    int sendRequest(HttpRequest::ptr req);

public:
    // 核心方法：简化httpConnection的使用。timeout_ms是超时时间，性能考虑，不能无限制的请求，比如前端已经不再需要请求，后端则也不再需要
    static HttpResult::ptr DoRequest(HttpMethod method
                                    , const std::string &url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");
    // 核心方法：传入Uri对象。
    static HttpResult::ptr DoRequest(HttpMethod method
                                    , Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");
    // 核心方法：可能是收到客户端的请求，将请求做了一些小改动。还要再请求别的服务器。所以还需要uri来定位
    static HttpResult::ptr DoRequest(HttpRequest::ptr req
                                    , Uri::ptr uri
                                    , uint64_t timeout_ms);
    // 以下几个是为了简化使用，可以用特定的方式请求
    static HttpResult::ptr DoGet(const std::string &url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");

    static HttpResult::ptr DoGet(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");

    static HttpResult::ptr DoPost(const std::string &url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");

    static HttpResult::ptr DoPost(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");

};

}
}

#endif