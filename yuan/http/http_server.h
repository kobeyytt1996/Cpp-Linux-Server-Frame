#ifndef __YUAN_HTTP_SERVER_H__
#define __YUAN_HTTP_SERVER_H__
/**
 * @file http_server.h
 * 继承TcpServer。用HttpSession解析客户端请求，再交给Servlet管理类找到对应的Servlet处理业务逻辑，获取响应报文，再用HttpSession返回响应
 * 使用者提供具体的Servlet即可
 */

#include "../tcp_server.h"
#include "http_session.h"
#include "servlet.h"

namespace yuan {
namespace http {

// 处理http协议的服务器
class HttpServer : public TcpServer {
public:
    typedef std::shared_ptr<HttpServer> ptr;
    HttpServer(bool keepAlive = false
        , IOManager *worker = IOManager::GetThis(), IOManager *accept_worker = IOManager::GetThis());

    ServletDispatch::ptr getServletDispatch() const { return m_dispach; }
    void setServletDispatch(ServletDispatch::ptr dispatch) { m_dispach = dispatch; }
protected:    
    void handleClient(Socket::ptr client) override;

private:
    // 是否支持长连接
    bool m_isKeepAlive;
    ServletDispatch::ptr m_dispach;
};

}
}

#endif