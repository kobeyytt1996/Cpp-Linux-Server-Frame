#ifndef __YUAN_SOCKET_STREAM_H__
#define __YUAN_SOCKET_STREAM_H__

#include "socket.h"
#include "stream.h"

namespace yuan {

// socket头文件只是针对socket的C API封装，这里是针对业务级接口封装。
// 各种协议都可以实现此类，比如Http，在读写数据时可以增加Http报文文本结构相关的实现
class SocketStream : public Stream {
public:
    typedef std::shared_ptr<SocketStream> ptr;

    SocketStream(Socket::ptr &sock, bool isOwner = true);
    // 如果m_isOwner为true。则销毁时要断开连接。主要针对短连接
    ~SocketStream() override;

    int read(void *buffer, int length) override;
    int read(ByteArray::ptr ba, int length) override;

    int write(const void *buffer, int length) override;
    int write(ByteArray::ptr ba, int length) override;

    void close() override;

    bool isConnected() const;
    Socket::ptr getSocket() const { return m_socket; }
protected:
    Socket::ptr m_socket;
    // 标记是否由此stream管理该socket。如果是的话，则stream析构的时候也要负责关闭socket。不是则只对socket作输入输出
    bool m_isOwner;
};

}

#endif