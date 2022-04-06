#ifndef __YUAN_SOCKET_STREAM_H__
#define __YUAN_SOCKET_STREAM_H__

#include "socket.h"
#include "stream.h"

namespace yuan {

// socket头文件只是针对socket的C API封装，这里是针对业务级接口封装
class SocketStream : public Stream {
public:
    typedef std::shared_ptr<SocketStream> ptr;

    SocketStream(Socket::ptr &sock, bool isOwner = true);
    ~SocketStream() override;

    int read(void *buffer, int length) override;
    int read(ByteArray::ptr ba, int length) override;

    int write(const void *buffer, int length) override;
    int write(ByteArray::ptr ba, int length) override;

    void close() override;

    Socket::ptr getSocket() const { return m_socket; }
private:
    Socket::ptr m_socket;
    // 标记是否由此stream管理该socket。如果是的话，则stream析构的时候也要负责关闭socket。不是则只对socket作输入输出
    bool m_isOwner;
};

}

#endif