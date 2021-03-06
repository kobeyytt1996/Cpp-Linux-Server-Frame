#ifndef __YUAN_TCP_SERVER_H__
#define __YUAN_TCP_SERVER_H__

/**
 * 封装了通用的Tcp服务器架构
 * 创建socket->bind->listen->acccept。用iomanager调度处理客户端的请求
 * 具体的实现类现在有：HttpServer
 */

#include <functional>
#include <memory>
#include <vector>

#include "address.h"
#include "iomanager.h"
#include "noncopyable.h"
#include "socket.h"

namespace yuan {

// 基类，可以让各类基于tcp的应用层协议进行扩展，如HttpServer
// 注：enable_shared_from_this的原理：https://segmentfault.com/a/1190000020861953
class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable {
public:
    typedef std::shared_ptr<TcpServer> ptr;
    TcpServer(IOManager *worker = IOManager::GetThis(), IOManager *accept_worker = IOManager::GetThis());
    // 可以作为基类让不同的协议进行扩展。有使用socket，就要把使用的都关闭掉
    virtual ~TcpServer();

    // 无论是哪种IP协议，IPv4或IPv6，都可以支持bind
    virtual bool bind(yuan::Address::ptr addr);
    // 支持多个地址的bind。也是上面的bind的具体实现.
    // 传入的多个地址可能只有部分可以bind。比如有一些端口号已被占用.需要调用者循环调用此方法，直到绑定成功。
    virtual bool bind(const std::vector<yuan::Address::ptr> &addrs, std::vector<Address::ptr> &fails);
    // 启动服务器。即开始accept
    virtual bool start();
    // 停止服务器
    virtual void stop();

    uint64_t getRecvTimeout() const { return m_recvTimeout; }
    const std::string getName() const { return m_name; }
    bool isStop() const { return m_isStop; } 
    
    void setRecvTimeout(uint64_t recvTimeout) { m_recvTimeout = recvTimeout; }
    void setName(const std::string &name) { m_name = name; }
protected:
    // 每当accept到一个socket，就调用该方法进行处理。注意是在搭建一个框架，这个方法的具体实现应该由使用者（子类）来决定。业务逻辑等应该放在这里
    virtual void handleClient(Socket::ptr client);
    // 因为监听多个端口，所以也要传具体哪一个端口要开始accept
    virtual void startAccept(Socket::ptr sock);
private:
    // 存储listen的socket。会支持多协议，可以同时监听多地址。比如，有两个网卡，一个内网一个外网
    std::vector<Socket::ptr> m_socks;
    // 当作线程池来使用。socket accept之后，要交给线程池处理
    IOManager *m_worker;
    // 和m_worker作区分。accept本身不耗时，accept之后处理客户的请求才耗时。所以accept可以单独放在一个线程里
    IOManager *m_acceptWorker;
    // 要防止无效的或者恶意的连接。除了在报文格式上要检查，这里也要增加超时时间，防止fd等资源被浪费
    uint64_t m_recvTimeout;
    // 可能会有多个server，根据名字来在log的时候做区分
    std::string m_name;
    // Server是否已停止
    bool m_isStop;
};

}

#endif