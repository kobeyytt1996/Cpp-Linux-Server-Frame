#ifndef __YUAN_HTTP_SESSION_H__
#define __YUAN_HTTP_SESSION_H__
/**
 * 通常的命名规则：HttpSession指服务端accept请求之后的socket
 * HttpConnection指客户端要建立连接的socket
 */
#include "../socket_stream.h"
#include "http.h"

namespace yuan {
namespace http {

class HttpSession : public SocketStream {
public:
    typedef std::shared_ptr<HttpSession> ptr;
    HttpSession(Socket::ptr &sock, bool isOwner = true);

    HttpRequest::ptr recvRequest();
    int sendResponse(HttpResponse::ptr resp);

};

}
}

#endif