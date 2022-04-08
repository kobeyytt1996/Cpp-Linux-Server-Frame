#include "http_server.h"
#include "../log.h"

static yuan::Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

namespace yuan {
namespace http {

HttpServer::HttpServer(bool keepAlive, IOManager *worker, IOManager *accept_worker) 
    : TcpServer(worker, accept_worker)
    , m_isKeepAlive(keepAlive)
    , m_dispach(std::make_shared<ServletDispatch>()) {}
  
void HttpServer::handleClient(Socket::ptr client) {
    HttpSession::ptr session = std::make_shared<HttpSession>(client);
    do {
        auto req = session->recvRequest();
        if (!req) {
            YUAN_LOG_WARN(g_system_logger) << "recv http request fail, errno=" << errno 
                << " errstr=" << strerror(errno) << " client:" << *client;
            break; 
        }
        // 如果客户端请求的头部是非长连接或者服务器不支持长连接，都把报文里的字段设置为关闭长连接
        HttpResponse::ptr resp(new HttpResponse(req->getVersion(), req->isClose() || !m_isKeepAlive));
        // 交给Servlet管理类来处理
        m_dispach->handle(req, resp, session);
        // resp->setBody("hello yuan");

        // YUAN_LOG_INFO(g_system_logger) << "request:" << std::endl << *req;
        // YUAN_LOG_INFO(g_system_logger) << "response:" << std::endl << *resp;

        session->sendResponse(resp);
    } while (m_isKeepAlive);
    session->close();
}

}
}