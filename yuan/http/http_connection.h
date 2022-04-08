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

class HttpConnection : public SocketStream {
public:
    typedef std::shared_ptr<HttpConnection> ptr;
    HttpConnection(Socket::ptr &sock, bool isOwner = true);

    // 核心方法：即根据Http协议特点解析http响应
    HttpResponse::ptr recvResponse();
    int sendRequest(HttpRequest::ptr req);

};

}
}

#endif