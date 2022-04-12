#include "http_parser.h"
#include "http_connection.h"
#include "../log.h"

namespace yuan {
namespace http {

static yuan::Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

std::string HttpResult::toString() const {
    std::stringstream ss;
    ss << "[HttpResult result=" << static_cast<int>(result)
        << " error=" << error
        << " response=" << (response ? response->toString() : "nullptr")
        << "]";
    return ss.str();
}

/**
 *  以下为HttpConnection的函数实现
 */
HttpConnection::HttpConnection(Socket::ptr &sock, bool isOwner)
    : SocketStream(sock, isOwner) {}

HttpConnection::~HttpConnection() {
    YUAN_LOG_DEBUG(g_system_logger) << "~HttpConnnection";
}

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
            // 所有return nullptr的地方都要加close，说明出问题了，要及时释放资源
            close();
            return nullptr;
        }

        // len是所有待解析数据的总长度
        len += offset;
        // rl文件里httpclient_parser_execute方法要求data以'\0'结尾
        data[len] = '\0';
        size_t parsed_len = parser->execute(data, len, false);
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

    const httpclient_parser &client_parser = parser->getParser();
    // 以下为获取响应体。分成chunked编码格式和Content-Length两种传递格式获取：https://blog.csdn.net/whatday/article/details/7571451
    if (client_parser.chunked) {
        std::string body;
        // 第一个循环是为了处理多个chunk，里面每次处理一个chunk
        do {
            // 里面的循环确保一定能拿到单个chunk的长度（即获取chunk头部），拿到长度则退出循环，处理该chunk正文
            do {
                int len = read(data + offset, buff_size - offset);
                if (len <= 0) {
                    close();
                    return nullptr;
                }
                // len是data里待解析的数据总长度
                len += offset;
                data[len] = '\0';
                size_t parsed_len = parser->execute(data, len, true);
                if (parser->hasError()) {
                    close();
                    return nullptr;
                }
                offset = len - parsed_len;
                if (offset == (int)buff_size) {
                    close();
                    return nullptr;
                }
            } while (!parser->isFinished()); 

            // 开始读出chunk正文。注意：chunk正文和下个chunk头部之间还有/r/n，这两个字符parser解析会出错，在下面要手动处理它们
            // client_parser.content_len代表当前要解析的chunk正文的数据长度，不包含\r\n
            int chunk_len = client_parser.content_len;
            // chunk_total_len指加上\r\n
            int chunk_total_len = chunk_len + 2;
            if (chunk_total_len <= offset) {
                body.append(data, chunk_len);
                memmove(data, data + chunk_total_len, offset - chunk_total_len);
                offset -= chunk_total_len;
            } else {
                // 说明chunk中还有数据没有读到data中
                int left_chunk = chunk_total_len - offset;
                body.append(data, offset);
                offset = 0;
                while (left_chunk > 0) {
                    int len = read(data, left_chunk > (int)buff_size ? (int)buff_size : left_chunk);
                    if (len <= 0) {
                        close();
                        return nullptr;
                    }
                    body.append(data, len);
                    left_chunk -= len;
                }
                // \r\n只是两个chunk的分隔符，要去掉
                body.resize(body.size() - 2);
            }
        } while (!client_parser.chunks_done);

        parser->getData()->setBody(body);
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
                    close();
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

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method
                                    , const std::string &url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers
                                    , const std::string &body) {
    Uri::ptr uri = Uri::CreateUri(url); 
    if (!uri) {
        return std::make_shared<HttpResult>(HttpResult::Result::INVALID_URL
            , nullptr, "invalid url: " + url);
    }   
    return DoRequest(method, uri, timeout_ms, headers, body);                                
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method
                                , Uri::ptr uri
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string> &headers
                                , const std::string &body){
    HttpRequest::ptr req = std::make_shared<HttpRequest>();
    req->setMethod(method);
    req->setPath(uri->getPath());
    req->setQuery(uri->getQuery());
    req->setFragment(uri->getFragment());  

    // 请求头需特殊处理connection和host
    bool has_host = false;
    for (auto &header : headers) {
        if (strcasecmp(header.first.c_str(), "connection") == 0) {
            if (strcasecmp(header.second.c_str(), "keep-alive") == 0) {
                req->setClose(false);
            }
            continue;
        }
        if (!has_host && strcasecmp(header.first.c_str(), "host") == 0) {
            has_host = !(header.second.empty());
        }
        req->setHeader(header.first, header.second);
    }
    if (!has_host) {
        req->setHeader("Host", uri->getHost());
    }

    req->setBody(body);

    return DoRequest(req, uri, timeout_ms);                             
}

HttpResult::ptr HttpConnection::DoRequest(HttpRequest::ptr req
                                , Uri::ptr uri
                                , uint64_t timeout_ms) {
    Address::ptr addr = uri->createAddress();    
    if (!addr) {
        return std::make_shared<HttpResult>(HttpResult::Result::INVALID_HOST
            , nullptr, "invalid host: " + uri->getHost());
    }
    Socket::ptr sock = Socket::CreateTCP(addr);
    if (!sock->connect(addr, timeout_ms)) {
        return std::make_shared<HttpResult>(HttpResult::Result::CONNECT_FAIL
            , nullptr, "connect fail with: " + addr->toString());
    }
    sock->setRecvTimeout(timeout_ms);
    HttpConnection::ptr conn = std::make_shared<HttpConnection>(sock);
    int ret = conn->sendRequest(req);    
    // TODO:从man send来看，send和recv不同，返回值应该不会有0.这里是否还要这个判断有待商榷
    if (ret == 0) {
        return std::make_shared<HttpResult>(HttpResult::Result::SEND_CLOSE_BY_PEER
            , nullptr, "send close by peer: " + addr->toString());
    }
    if (ret < 0) {
        return std::make_shared<HttpResult>(HttpResult::Result::SEND_SOCKET_ERROR
            , nullptr, "send socket error to: " + addr->toString() + " errno="
            + std::to_string(errno) + " strerr=" + strerror(errno));
    }
    auto resp = conn->recvResponse();
    if (!resp) {
        return std::make_shared<HttpResult>(HttpResult::Result::RECV_TIMEOUT_OR_OTHER_ERROR
            , nullptr, "recv response timeout or other error from: " + addr->toString() 
            + " timeout_ms: " + std::to_string(timeout_ms));
    }
    return std::make_shared<HttpResult>(HttpResult::Result::OK
            , resp, "ok");
}

HttpResult::ptr HttpConnection::DoGet(const std::string &url
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string> &headers
                                , const std::string &body) {
    return DoRequest(HttpMethod::GET, url, timeout_ms, headers, body);                                                                         
}

HttpResult::ptr HttpConnection::DoGet(Uri::ptr uri
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string> &headers
                                , const std::string &body) {
    return DoRequest(HttpMethod::GET, uri, timeout_ms, headers, body);                                    
}


HttpResult::ptr HttpConnection::DoPost(const std::string &url
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string> &headers
                                , const std::string &body) {
    return DoRequest(HttpMethod::POST, url, timeout_ms, headers, body);                                  
}


HttpResult::ptr HttpConnection::DoPost(Uri::ptr uri
                                , uint64_t timeout_ms
                                , const std::map<std::string, std::string> &headers
                                , const std::string &body) {
    return DoRequest(HttpMethod::GET, uri, timeout_ms, headers, body);                                     
}

/**
 *  以下为HttpConnectionPool的函数实现
 */
HttpConnectionPool::HttpConnectionPool(const std::string &host
        , const std::string &vhost
        , uint16_t port
        , uint32_t max_size
        , uint32_t max_alive_time
        , uint32_t max_request) 
            : m_host(host)
            , m_vhost(vhost)
            , m_port(port)
            , m_maxSize(max_size)
            , m_maxAliveTime(max_alive_time)
            , m_maxRequest(max_request) {}

HttpConnection::ptr HttpConnectionPool::getConnection() {
    uint64_t now_time_ms = yuan::GetCurrentTimeMS();
    std::vector<HttpConnection*> invalid_connections;

    HttpConnection *conn = nullptr;
    MutexType::Lock lock(m_mutex);
    while (!m_conns.empty()) {
        HttpConnection *temp = m_conns.front();
        m_conns.pop_front();
        if (!temp->isConnected()) {
            // 细节：连接已关闭。则要移除连接。连接池里存的是裸指针，则要收集到容器里再统一delete
            invalid_connections.push_back(temp);
            continue;
        }
        if (temp->m_createTime + m_maxAliveTime <= now_time_ms) {
            // 已超时，要关闭连接
            invalid_connections.push_back(temp);
            continue;
        }
        conn = temp;
        break;
    }
    lock.unlock();

    for (auto &invalid : invalid_connections) {
        delete invalid;
    }
    m_total -= invalid_connections.size();

    // 要创建新连接
    if (!conn) {
        IPAddress::ptr addr = Address::LookupAnyIPAdress(m_host);
        if (!addr) {
            YUAN_LOG_ERROR(g_system_logger) << "get addr failed, host: " << m_host;
            return nullptr;
        }
        addr->setPort(m_port);
        Socket::ptr sock = Socket::CreateTCP(addr);
        if (!sock->connect(addr)) {
            YUAN_LOG_ERROR(g_system_logger) << "connect failed, host: " << m_host << " port:" << std::to_string(m_port);
            return nullptr;
        }
        conn = new HttpConnection(sock);
        // 别忘了设置创建时间
        conn->m_createTime = now_time_ms;
        ++m_total;
    }
    // 注意这里对bind的使用
    return HttpConnection::ptr(conn, std::bind(&ReleasePtr, std::placeholders::_1, this));
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method
                        , const std::string &url
                        , uint64_t timeout_ms
                        , const std::map<std::string, std::string> &headers
                        , const std::string &body) {
    HttpRequest::ptr req = std::make_shared<HttpRequest>();
    req->setMethod(method);
    // 注意：这里虽然只调用了setPath，但看下面的deRequest方法，url里已包含path,query,fragment，所以相当于都设置了
    req->setPath(url);
    // 重要：一定在头部加上长连接，否则会被服务器断开连接
    req->setClose(false);

    // 请求头需特殊处理connection和host
    bool has_host = false;
    for (auto &header : headers) {
        if (strcasecmp(header.first.c_str(), "connection") == 0) {
            if (strcasecmp(header.second.c_str(), "keep-alive") == 0) {
                req->setClose(false);
            }
            continue;
        }
        if (!has_host && strcasecmp(header.first.c_str(), "host") == 0) {
            has_host = !(header.second.empty());
        }
        req->setHeader(header.first, header.second);
    }
    // 注意：这里和普通连接的处理逻辑不同
    if (!has_host) {
        if (m_vhost.empty()) {
            req->setHeader("Host", m_host);
        } else {
            req->setHeader("Host", m_vhost);
        }
    }

    req->setBody(body);

    return doRequest(req, timeout_ms);                        
}
 
HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method
                        , Uri::ptr uri
                        , uint64_t timeout_ms
                        , const std::map<std::string, std::string> &headers
                        , const std::string &body) {
    std::stringstream ss;
    ss << uri->getPath()
        << (uri->getQuery().empty() ? "" : "?")
        << uri->getQuery()
        << (uri->getQuery().empty() ? "" : "#")  
        << uri->getFragment();
    return doRequest(method, ss.str(), timeout_ms, headers, body);                     
}
 
HttpResult::ptr HttpConnectionPool::doRequest(HttpRequest::ptr req
                        , uint64_t timeout_ms) {
    // 注意：和普通连接的区别就在于获取连接的方法
    HttpConnection::ptr conn = getConnection();
    if (!conn) {
        return std::make_shared<HttpResult>(HttpResult::Result::POOL_GET_CONNECTION_FAIL
            , nullptr, "get connection from pool fail, peer: " + m_host + ":" + std::to_string(m_port));
    }
    Socket::ptr sock = conn->getSocket(); 
    if (!sock) {
        return std::make_shared<HttpResult>(HttpResult::Result::POOL_GET_CONNECTION_FAIL
            , nullptr, "invalid connection in pool, peer: " + m_host + ":" + std::to_string(m_port));
    }
    sock->setRecvTimeout(timeout_ms);
    int ret = conn->sendRequest(req);    
    // TODO:从man send来看，send和recv不同，返回值应该不会有0.这里是否还要这个判断有待商榷
    if (ret == 0) {
        return std::make_shared<HttpResult>(HttpResult::Result::SEND_CLOSE_BY_PEER
            , nullptr, "send close by peer: " + m_host + ":" + std::to_string(m_port));
    }
    if (ret < 0) {
        return std::make_shared<HttpResult>(HttpResult::Result::SEND_SOCKET_ERROR
            , nullptr, "send socket error to: " + m_host + ":" + std::to_string(m_port)
             + " errno=" + std::to_string(errno) + " strerr=" + strerror(errno));
    }
    auto resp = conn->recvResponse();
    if (!resp) {
        return std::make_shared<HttpResult>(HttpResult::Result::RECV_TIMEOUT_OR_OTHER_ERROR
            , nullptr, "recv response timeout or other error from: " 
            + m_host + ":" + std::to_string(m_port) 
            + " timeout_ms: " + std::to_string(timeout_ms));
    }
    return std::make_shared<HttpResult>(HttpResult::Result::OK, resp, "ok");                       
}

HttpResult::ptr HttpConnectionPool::doGet(const std::string &url
                    , uint64_t timeout_ms
                    , const std::map<std::string, std::string> &headers
                    , const std::string &body) {
    return doRequest(HttpMethod::GET, url, timeout_ms, headers, body);                           
}

HttpResult::ptr HttpConnectionPool::doGet(Uri::ptr uri
                    , uint64_t timeout_ms
                    , const std::map<std::string, std::string> &headers
                    , const std::string &body) {
    return doRequest(HttpMethod::GET, uri, timeout_ms, headers, body);                                                
}

HttpResult::ptr HttpConnectionPool::doPost(const std::string &url
                    , uint64_t timeout_ms
                    , const std::map<std::string, std::string> &headers
                    , const std::string &body) {
    return doRequest(HttpMethod::POST, url, timeout_ms, headers, body);                                                
}

HttpResult::ptr HttpConnectionPool::doPost(Uri::ptr uri
                    , uint64_t timeout_ms
                    , const std::map<std::string, std::string> &headers
                    , const std::string &body) {
    return doRequest(HttpMethod::POST, uri, timeout_ms, headers, body);                        
}


void HttpConnectionPool::ReleasePtr(HttpConnection *ptr, HttpConnectionPool *pool) {
    if (!ptr->isConnected()
        || ptr->m_createTime + pool->m_maxAliveTime <= yuan::GetCurrentTimeMS()) {
            delete ptr;
            return;
    }     
    MutexType::Lock lock(pool->m_mutex);
    pool->m_conns.push_back(ptr);                   
}

}
}