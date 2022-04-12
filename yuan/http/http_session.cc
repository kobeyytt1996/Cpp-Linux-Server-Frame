#include "http_parser.h"
#include "http_session.h"

namespace yuan {
namespace http {


HttpSession::HttpSession(Socket::ptr &sock, bool isOwner)
    : SocketStream(sock, isOwner) {}

HttpRequest::ptr HttpSession::recvRequest() {
    HttpRequestParser::ptr parser(new HttpRequestParser());
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    // uint64_t buff_size = 100;
    std::shared_ptr<char> buffer(new char[buff_size], [](char *ptr){
        delete [] ptr;
    });

    char *data = buffer.get();
    int offset = 0;
    // 注意两点：parser只解析请求行和请求头。每次execute后，data存的是未解析的数据，offset存的是data里未解析数据的长度
    while (true) {
        int len = read(data + offset, buff_size - offset);
        if (len <= 0) {
            close();
            return nullptr;
        }

        // len是所有待解析数据的总长度
        len += offset;
        size_t parsed_len = parser->execute(data, len);
        if (parser->hasError()) {
            close();
            return nullptr;
        }
        offset = len - parsed_len;
        // 所有数据均未解析。没有空间再读取数据。说明是某个头部字段过长，为了避免恶意请求。不再解析，直接返回
        if (offset == (int)buff_size) {
            close();
            return nullptr;
        }
        if (parser->isFinished()) {
            break;
        }
    }

    // 以下为获取请求体。可能两个来源：data未解析的部分和还没有read到的数据
    int64_t body_len = parser->getContentLength();
    if (body_len > 0) {
        std::string body;
        body.reserve(body_len);

        body.append(data, std::min(body_len, (int64_t)offset));
        int64_t body_offset = body.size();
        if (body_len > body_offset) {
            body.resize(body_len);
            if (readFixSize(&body[body_offset], body_len - body_offset) <= 0) {
                close();
                return nullptr;
            }
        }

        parser->getData()->setBody(body);
    }
    // 易错：要处理客户端发来的头部长连接信息
    std::string keep_alive = parser->getData()->getHeader("Connection");
    if (!strcasecmp(keep_alive.c_str(), "keep-alive")) {
        parser->getData()->setClose(false);
    }
    return parser->getData();
}

int HttpSession::sendResponse(HttpResponse::ptr resp) {
    std::string data = resp->toString();
    return writeFixSize(data.c_str(), data.size());
}

}
}