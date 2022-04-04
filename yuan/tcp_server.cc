#ifndef __YUAN_TCP_SERVER_H__
#define __YUAN_TCP_SERVER_H__

#include <functional>
#include <memory>
#include <vector>

#include "address.h"
#include "iomanager.h"
#include "socket.h"

namespace yuan {

class TcpServer : public std::enable_shared_from_this<TcpServer> {
public:
    typedef std::shared_ptr<TcpServer> ptr;
    TcpServer(IOManager *worker = IOManager::GetThis());
    // 可以作为基类让不同的协议进行扩展
    virtual ~TcpServer() {}

    virtual bool bind(yuan::Address::ptr addr);
    // 支持多个地址的bind
    virtual bool bind(std::vector<yuan::Address::ptr> addrs);
    // 启动服务器
    virtual bool start();
    // 停止服务器
    virtual bool stop();

    uint64_t getReadTimeout() const { return m_readTimeout; }
    const std::string getName() const { return m_name; }
    bool isStop() const { return m_isStop; } 
    
    void setReadTimeout(uint64_t readTimeout) { m_readTimeout = readTimeout; }
    void setName(const std::string &name) { m_name = name; }
protected:
    // 每当accept到一个socket，就调用该方法进行处理
    virtual void handleClient(Socket::ptr client);
private:
    // 存储listen的socket。会支持多协议，可以同时监听多地址。比如，有两个网卡，一个内网一个外网
    std::vector<Socket::ptr> m_socks;
    // 当作线程池来使用。socket accept之后，要交给线程池处理
    IOManager *m_worker;
    // 要防止无效的或者恶意的连接。除了在报文格式上要检查，这里也要增加超时时间，防止fd等资源被浪费
    uint64_t m_readTimeout;
    // 可能会有多个server，根据名字来在log的时候做区分
    std::string m_name;
    // Server是否已停止
    bool m_isStop;
};

}

#endif