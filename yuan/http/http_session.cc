#include "http_parser.h"
#include "http_session.h"

namespace yuan {
namespace http {


HttpSession::HttpSession(Socket::ptr &sock, bool isOwner)
    : SocketStream(sock, isOwner) {}

HttpRequest::ptr HttpSession::recvRequest() {
    HttpRequestParser::ptr parser(new HttpRequestParser());
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    std::shared_ptr<char> buffer(new char[buff_size], [](char *ptr){
        delete [] ptr;
    });

    char *data = buffer.get();
    int len = read(data, buff_size);
    if (len <= 0) {
        return nullptr;
    }

    size_t n_parse = parser->execute(data, len);
    if (parser->hasError()) {
        return nullptr;
    }
}

int HttpSession::sendResponse(HttpResponse::ptr resp) {

}

}
}