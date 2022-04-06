#ifndef __YUAN_STREAM_H__
#define __YUAN_STREAM_H__

#include <memory>

#include "bytearray.h"

namespace yuan {

// 对文件和socket读写的封装。是基类。
class Stream {
public:
    typedef std::shared_ptr<Stream> ptr;
    // 文件和socket会有不同的实现
    virtual ~Stream();

    virtual int read(void *buffer, int length) = 0;
    // 向pa的position处写入length个字节
    virtual int read(ByteArray::ptr ba, int length) = 0;
    // 重点：读到参数指定的length个字节才返回。比如：已经从http头部知晓了Content-Length，所以指定读这么多字节
    virtual int readFixSize(void *buffer, int length);
    virtual int readFixSize(ByteArray::ptr ba, int length);

    virtual int write(const void *buffer, int length) = 0;
    // 从pa的position处读入length个字节
    virtual int write(ByteArray::ptr ba, int length) = 0;
    virtual int writeFixSize(const void *buffer, int length);
    virtual int writeFixSize(ByteArray::ptr ba, int length);

    virtual void close() = 0;

private:


};

}

#endif