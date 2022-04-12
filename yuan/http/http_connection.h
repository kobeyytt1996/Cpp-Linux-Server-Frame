#ifndef __YUAN_HTTP_Connection_H__
#define __YUAN_HTTP_Connection_H__
/**
 * 通常的命名规则：HttpSession指服务端accept请求之后的socket
 * HttpConnection指客户端要建立连接的socket
 * 
 * 这个头文件里还包含了连接池
 */
#include "../socket_stream.h"
#include "http.h"
#include "../thread.h"
#include "../uri.h"

#include <atomic>
#include <list>

namespace yuan {
namespace http {

// 用一个结构体更方便的封装http请求后获得的结果
struct HttpResult {
    typedef std::shared_ptr<HttpResult> ptr;
    enum class Result {
        OK = 0,
        INVALID_URL = 1,
        INVALID_HOST = 2,
        CONNECT_FAIL = 3,
        SEND_CLOSE_BY_PEER = 4,
        SEND_SOCKET_ERROR = 5,
        RECV_TIMEOUT_OR_OTHER_ERROR = 6,
        // 从连接池中取连接失败
        POOL_GET_CONNECTION_FAIL = 7,
        // 连接池中无效的连接
        POOL_INVALID_CONNECTION = 8
    };
    HttpResult(Result _result, HttpResponse::ptr _response, const std::string &_error) 
        : result(_result)
        , response(_response)
        , error(_error) {}
    Result result;
    HttpResponse::ptr response;
    // 说明具体哪里出现错误
    std::string error;

    std::string toString() const;
};

class HttpConnectionPool;

class HttpConnection : public SocketStream {
    friend class HttpConnectionPool;
public:
    typedef std::shared_ptr<HttpConnection> ptr;
    HttpConnection(Socket::ptr &sock, bool isOwner = true);
    // 为了后续连接池使用的时候调试方便
    ~HttpConnection() override;

    // 核心方法：即根据Http协议特点解析http响应
    HttpResponse::ptr recvResponse();
    int sendRequest(HttpRequest::ptr req);

public:
    // 核心方法：简化httpConnection的使用。timeout_ms是接收响应超时时间，性能考虑，不能无限制的请求，比如前端已经不再需要请求，后端则也不再需要
    static HttpResult::ptr DoRequest(HttpMethod method
                                    , const std::string &url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");
    // 核心方法：传入Uri对象。
    static HttpResult::ptr DoRequest(HttpMethod method
                                    , Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");
    // 核心方法：是其他方法的最终实现。也可能直接使用，可能是收到客户端的请求，将请求做了一些小改动。还要再请求别的服务器。所以还需要uri来定位
    static HttpResult::ptr DoRequest(HttpRequest::ptr req
                                    , Uri::ptr uri
                                    , uint64_t timeout_ms);
    // 以下几个是为了简化使用，可以用特定的方式请求
    static HttpResult::ptr DoGet(const std::string &url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");

    static HttpResult::ptr DoGet(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");

    static HttpResult::ptr DoPost(const std::string &url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");

    static HttpResult::ptr DoPost(Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string> &headers = {}
                                    , const std::string &body = "");

private:
    // 是给连接池里的连接用的。记录连接的创建时间
    uint64_t m_createTime = 0;
};

// 重点：为了处理长连接，使用了连接池的概念。类似Nginx。一个连接池是针对一个Host加port的
// 以后可以进一步完善。提供完整的分布式的负载均衡的框架
class HttpConnectionPool {
public:
    typedef std::shared_ptr<HttpConnectionPool> ptr;
    typedef Mutex MutexType;

    HttpConnectionPool(const std::string &host
        , const std::string &vhost
        , uint16_t port
        , uint32_t max_size
        , uint32_t max_alive_time
        , uint32_t max_request);

    // 核心方法：从连接池里取出连接，要注意处理max_alive_time和max_request的两个限制。无可用连接则要创造连接。
    HttpConnection::ptr getConnection();

    // 以下几个请求的函数和HttpConnection里的基本一致。区别在于不需要url或uri里的host信息，因为连接池只针对某一特定Host
    HttpResult::ptr doRequest(HttpMethod method
                            , const std::string &url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string> &headers = {}
                            , const std::string &body = "");
    // 把uri转成url再调用上面的方法。因为uri里的host和port信息已不再需要
    HttpResult::ptr doRequest(HttpMethod method
                            , Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string> &headers = {}
                            , const std::string &body = "");
    // 不需要uri了，因为连接池存储了连接信息
    HttpResult::ptr doRequest(HttpRequest::ptr req
                            , uint64_t timeout_ms);
    // 
    HttpResult::ptr doGet(const std::string &url
                        , uint64_t timeout_ms
                        , const std::map<std::string, std::string> &headers = {}
                        , const std::string &body = "");

    HttpResult::ptr doGet(Uri::ptr uri
                        , uint64_t timeout_ms
                        , const std::map<std::string, std::string> &headers = {}
                        , const std::string &body = "");

    HttpResult::ptr doPost(const std::string &url
                        , uint64_t timeout_ms
                        , const std::map<std::string, std::string> &headers = {}
                        , const std::string &body = "");

    HttpResult::ptr doPost(Uri::ptr uri
                        , uint64_t timeout_ms
                        , const std::map<std::string, std::string> &headers = {}
                        , const std::string &body = "");
private:
    // 重点：连接池中获取HttpConnection后，用智能指针管理。重写智能指针的deleter，释放时调用下面函数，判断是否放回连接池。不放回再释放
    static void ReleasePtr(HttpConnection *ptr, HttpConnectionPool *pool);
    
private:
    // 一个连接池是针对一个Host的
    std::string m_host;
    // Http1.1规定请求体必须有host：https://blog.csdn.net/zcw4237256/article/details/79228887
    std::string m_vhost;
    uint16_t m_port;
    // 连接数的上限
    uint32_t m_maxSize;
    // 每条连接的最长存活时间
    uint32_t m_maxAliveTime;
    // 一条连接上最多处理的请求个数。当达到时，就关闭该连接。和上面的存活时间是共同起作用的两种策略，仿照Nginx的设计
    uint32_t m_maxRequest;

    MutexType m_mutex;
    // 连接池，存放空闲连接。用list是因为有大量删减操作。存放的是裸指针，不用智能指针的原因看上面的ReleasePtr函数。
    // 展示了用裸指针的一种情况，当析构函数和智能指针的deleter要做的事情不同时。
    std::list<HttpConnection*> m_conns;
    // 是可以超过m_maxSize的，比如一些突发情况下，连接池里的连接不够用了。但之后也要记得把多的释放掉。
    // m_maxSiz不可以设置的太小。否则会频繁的创建和释放，和短连接没什么区别
    std::atomic<int32_t> m_total = {0};
};

}
}

#endif