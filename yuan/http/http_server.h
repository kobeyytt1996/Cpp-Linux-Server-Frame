#ifndef __YUAN_HTTP_SERVER_H__
#define __YUAN_HTTP_SERVER_H__

#include "../tcp_server.h"
#include "http_session.h"

namespace yuan {
namespace http {

class HttpServer : public TcpServer {
public:
    typedef std::shared_ptr<HttpServer> ptr;
    HttpServer(bool keepAlive = false
        , IOManager *worker = IOManager::GetThis(), IOManager *accept_worker = IOManager::GetThis());

protected:    
    void handleClient(Socket::ptr client) override;

private:
    // 是否支持长连接
    bool m_isKeepAlive;
};

}
}

#endif