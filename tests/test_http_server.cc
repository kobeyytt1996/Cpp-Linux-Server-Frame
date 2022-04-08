#include "../yuan/http/http_server.h"
#include "../yuan/log.h"

void run() {
    yuan::Address::ptr addr = yuan::Address::LookupAny("0.0.0.0:8020");
    yuan::http::HttpServer::ptr server(new yuan::http::HttpServer(false));
    while (!server->bind(addr)) {
        sleep(2);
    }
    auto slt = server->getServletDispatch();
    slt->addServlet("/echo/xx", [](yuan::http::HttpRequest::ptr req
            , yuan::http::HttpResponse::ptr resp
            , yuan::http::HttpSession::ptr session) -> int32_t {
        resp->setBody(req->toString());
        return 0;
    });
    // 模糊匹配
    slt->addGlobServlet("/echo/*", [](yuan::http::HttpRequest::ptr req
            , yuan::http::HttpResponse::ptr resp
            , yuan::http::HttpSession::ptr session) -> int32_t {
        resp->setBody("Glob:\r\n" + req->toString());
        return 0;
    });
    server->start();
}

int main(int argc, char **argv) {
    yuan::IOManager iom(2);
    iom.schedule(run);

    return 0;
}