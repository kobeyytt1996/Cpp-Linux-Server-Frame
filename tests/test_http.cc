#include "../yuan/address.h"
#include "../yuan/log.h"
#include "../yuan/http/http.h"
#include "../yuan/yuan_all_headers.h"

void test_request() {
    yuan::http::HttpRequest::ptr req = std::make_shared<yuan::http::HttpRequest>();
    req->setHeader("host", "www.sylar.top");
    req->setBody("hello yuan");

    req->dump(std::cout) << std::endl;
}

void test_response() {
    yuan::http::HttpResponse::ptr resp = std::make_shared<yuan::http::HttpResponse>();
    resp->setClose(false);
    resp->setStatus(yuan::http::HttpStatus::NOT_FOUND);
    // 仅测试用
    resp->setHeader("X-X", "yuan");
    resp->setBody("hello recv");

    resp->dump(std::cout) << std::endl;
}

int main(int argc, char **argv) {
    // test_request();
    test_response();

    return 0;
}
