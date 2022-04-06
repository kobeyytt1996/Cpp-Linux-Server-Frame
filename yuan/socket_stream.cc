#include "socket_stream.h"

namespace yuan {

SocketStream::SocketStream(Socket::ptr &sock, bool isOwner)
    : m_socket(sock)
    , m_isOwner(isOwner) {

}

SocketStream::~SocketStream() {
    if (m_isOwner && m_socket) {
        m_socket->close();
    }
}

int SocketStream::read(void *buffer, int length)  {
    if (!isConnected()) {
        return -1;
    }
    return m_socket->recv(buffer, length);
}

int SocketStream::read(ByteArray::ptr ba, int length)  {
    if (!isConnected()) {
        return -1;
    }
    std::vector<iovec> iovs;
    ba->getWriteBuffers(iovs, length);

    int ret = m_socket->recv(&(iovs[0]), iovs.size());
    if (ret > 0) {
        // 必须手动刷新一下ba的position和size
        ba->setPosition(ba->getPosition() + ret);
    }
    return ret;
}

int SocketStream::write(const void *buffer, int length) {
    if (!isConnected()) {
        return -1;
    }
    return m_socket->send(buffer, length);
}

int SocketStream::write(ByteArray::ptr ba, int length) {
    if (!isConnected()) {
        return -1;
    }
    std::vector<iovec> iovs;
    ba->getReadBuffers(iovs, length);
    int ret = m_socket->send(&iovs[0], iovs.size());
    if (ret > 0) {
        ba->setPosition(ba->getPosition() + ret);
    }
    return ret;
}

bool SocketStream::isConnected() const {
    return m_socket && m_socket->isConnected();
}

void SocketStream::close() {
    if (isConnected()) {
        m_socket->close();
    }
}

}