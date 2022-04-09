#include "http_parser.h"
#include "http_connection.h"

namespace yuan {
namespace http {


HttpConnection::HttpConnection(Socket::ptr &sock, bool isOwner)
    : SocketStream(sock, isOwner) {}

HttpResponse::ptr HttpConnection::recvResponse() {
    HttpResponseParser::ptr parser(new HttpResponseParser());
    uint64_t buff_size = HttpResponseParser::GetHttpResponseBufferSize();
    // uint64_t buff_size = 100;
    // 这里+1是因为rl文件里httpclient_parser_execute方法要求data以'\0'结尾
    std::shared_ptr<char> buffer(new char[buff_size + 1], [](char *ptr){
        delete [] ptr;
    });

    char *data = buffer.get();
    int offset = 0;
    // 注意两点：parser只解析响应行和响应头。每次execute后，data存的是未解析的数据，offset存的是data里未解析(剩余)数据的长度
    while (true) {
        int len = read(data + offset, buff_size - offset);
        if (len <= 0) {
            return nullptr;
        }

        // len是所有待解析数据的总长度
        len += offset;
        // rl文件里httpclient_parser_execute方法要求data以'\0'结尾
        data[len] = '\0';
        size_t parsed_len = parser->execute(data, len, false);
        if (parser->hasError()) {
            return nullptr;
        }
        offset = len - parsed_len;
        // 所有数据均未解析。没有空间再读取数据。说明是某个头部字段过长，为了避免恶意请求。不再解析，直接返回
        if (offset == (int)buff_size) {
            return nullptr;
        }
        if (parser->isFinished()) {
            break;
        }
    }

    httpclient_parser client_parser = parser->getParser();
    // 以下为获取响应体。分成chunked编码格式和Content-Length两种传递格式获取：https://blog.csdn.net/whatday/article/details/7571451
    if (client_parser.chunked) {
        std::string body;
        // 第一个循环是为了处理多个chunk，里面每次处理一个chunk
        do {
            // 里面的循环确保一定能拿到单个chunk的长度，拿到长度则退出循环，处理该chunk
            do {
                int len = read(data + offset, buff_size - offset);
                if (len <= 0) {
                    return nullptr;
                }
                // len是data里待解析的数据总长度
                len += offset;
                size_t parsed_len = parser->execute(data, len, true);
                if (parser->hasError()) {
                    return nullptr;
                }
                offset = len - parsed_len;
                if (offset == (int)buff_size) {
                    return nullptr;
                }
            } while (!parser->isFinished()); 
            // client_parser.content_len代表当前要解析的chunk的数据长度
            int chunk_len = client_parser.content_len;
            if (chunk_len <= offset) {
                body.append(data, chunk_len);
                memmove(data, data + chunk_len, offset - chunk_len);
                offset -= chunk_len;
            } else {
                // 说明chunk中还有数据没有读到data中
                int left_chunk = chunk_len - offset;
                body.append(data, offset);
                offset = 0;
                while (left_chunk > 0) {
                    int len = read(data, left_chunk > (int)buff_size ? buff_size : left_chunk);
                    if (len <= 0) {
                        return nullptr;
                    }
                    body.append(data, len);
                    left_chunk -= len;
                }
            }
        } while (!client_parser.chunks_done);
    } else {
         // Content-Length的方式：可能两个来源：data未解析的部分和还没有read到的数据
        int64_t body_len = parser->getContentLength();
        if (body_len > 0) {
            std::string body;
            body.reserve(body_len);

            body.append(data, std::min(body_len, (int64_t)offset));
            int64_t body_offset = body.size();
            if (body_len > body_offset) {
                body.resize(body_len);
                if (readFixSize(&body[body_offset], body_len - body_offset) <= 0) {
                    return nullptr;
                }
            }

            parser->getData()->setBody(body);
        }
    }
    
   
    return parser->getData();
}

int HttpConnection::sendRequest(HttpRequest::ptr req) {
    std::string data = req->toString();
    return writeFixSize(data.c_str(), data.size());
}

}
}