#ifndef __YUAN_HTTP_Connection_H__
#define __YUAN_HTTP_Connection_H__
/**
 * 通常的命名规则：HttpSession指服务端accept请求之后的socket
 * HttpConnection指客户端要建立连接的socket
 */
#include "../socket_stream.h"
#include "http.h"

namespace yuan {
namespace http {

// 用一个结构体更方便的封装http请求后获得的响应结果
struct HttpResult {
    typedef std::shared_ptr<HttpResult> ptr;
    HttpResult(int _result, HttpResponse::ptr _response, const std::string &_error) 
        : result(_result)
        , response(_response)
        , error(_error) {}
    int result;
    HttpResponse::ptr response;
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
    // 核心方法：简化httpConnection的使用。timeout_ms是超时时间，不能无限制的请求，比如前端已经不再需要请求，后端则也不再需要
    static HttpResult::ptr DoRequest(HttpMethod method
                                    , const std::string &url
                                    , uint64_t timeout_ms);
};

}
}

#endif