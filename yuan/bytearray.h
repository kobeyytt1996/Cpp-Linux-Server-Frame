#ifndef __YUAN_BYTEARRAY_H__
#define __YUAN_BYTEARRAY_H__
/**
 * 该类用于网络传输的序列化。做网络协议的解析，方便取数据和统一写入数据
 * 比如收数据就是获得一块数据的内存，这个类可以让想读int就读int，想读string就读string
 * 比如json是把所有类型统一序列化为了字符串，而这里是都序列化为二进制流
 * 和远端要约定好传了什么数据类型
 * 还涉及到了压缩算法
 */

#include <memory>
#include <stdint.h>
#include <string>
#include <sys/types.h>
#include <sys/uio.h>
#include <vector>

namespace yuan {

class ByteArray {
public:
    typedef std::shared_ptr<ByteArray> ptr;

    // 用链表来存储，Node代表一块内存空间，如果写满了则再分配一块。不使用数组，因为还要动态扩展且不一定能轻易的分配出连续的大空间
    struct Node {
        Node(size_t s);
        Node();
        ~Node();

        char *ptr;
        size_t size;
        Node *next;
    };

    ByteArray(size_t base_size = 4096);
    ~ByteArray();

    /**
     * 以下都为读写方法
     */
    // 以下为写入整数，F指针对固定长度
    void writeFint8(int8_t value);
    void writeFuint8(uint8_t value);
    void writeFint16(int16_t value);
    void writeFuint16(uint16_t value);
    void writeFint32(int32_t value);
    void writeFuint32(uint32_t value);
    void writeFint64(int64_t value);
    void writeFuint64(uint64_t value);

    // 重点：对数据压缩，生成可变长度。只处理32和64位，短的处理起来没意义。注意负数要特殊处理，否则压缩后字节数还可能变大
    // 算法具体：https://hebl.china-vo.org/w/19539684
    // 了解负数补码：https://mp.weixin.qq.com/s?__biz=MzA3MDExNzcyNA==&mid=2650392086&idx=1&sn=6a2ecfe2548f121a4726d03bf23f4478&scene=0#wechat_redirect
    void writeInt32(int32_t value);
    void writeUint32(uint32_t value);
    void writeInt64(int64_t value);
    void writeUint64(uint64_t value);

    // 由于浮点型的存储原理，不能压缩：https://mp.weixin.qq.com/s?__biz=MzI2OTA3NTk3Ng==&mid=2649283763&idx=1&sn=7244a95dcba1e3581851384cb3337650&chksm=f2f9afd4c58e26c27ec3ac39b8433d9f02998603a8000931fb6d5882f46c0a4e73cf784d6f74&scene=21#wechat_redirect
    void writeFloat(float value);
    void writeDouble(double value);

    // F16即指需要在前面添加16bit来描述字符串长度。即：length:int16, data
    void writeStringF16(const std::string &value);
    // length:int32, data
    void writeStringF32(const std::string &value);
    // length:int64, data
    void writeStringF64(const std::string &value);
    // 根据字符串长度压缩一个int来记录。length:varint, data
    void writeStringVint(const std::string &value);
    // 不记录长度，只有data
    void writeStringWithoutLength(const std::string &value);

    // 以下为读取固定长度的整数
    int8_t readFint8();
    uint8_t readFuint8();
    int16_t readFint16();
    uint16_t readFuint16();
    int32_t readFint32();
    uint32_t readFuint32();
    int64_t readFint64();
    uint64_t readFuint64();

    // 读取压缩后的整数
    int32_t readInt32();
    uint32_t readUint32();
    int64_t readInt64();
    uint64_t readUint64();

    // 读取浮点型
    float readFloat();
    double readDouble();

    // 读取字符串
    std::string readStringF16();
    std::string readStringF32();
    std::string readStringF64();
    std::string readStringVint();

    /**
     * 以下为一些内部操作函数
     */
    // 只保留一个节点，释放其他节点
    void clear();
    
    // 重点：向链表结构的内存里写数据
    void write(const void *buf, size_t size);
    // 读取数据，并移动指针
    void read(void *buf, size_t size);
    // 有点像peek的感觉，读取数据，但不改变m_position等成员变量的值
    void read(void *buf, size_t size, size_t position) const;

    size_t getPosition() const { return m_position; }
    void setPosition(size_t val);

    // 方便的写入文件，可以用来当数据有问题时，做调试
    bool writeToFile(const std::string &name) const;
    // 从文件读出
    bool readFromFile(const std::string &name);

    size_t getBaseSize() const { return m_baseSize; }
    // 获取还有多少数据可以读取
    size_t getReadSize() const { return m_size - m_position; }
    
    size_t getSize() const { return m_size; }

    // 判断字节序
    bool isLittleEndian() const;
    // 设置字节序
    void setIsLittleEndian(bool is_little);

    // 将还没读出的数据（二进制流）转为string
    const std::string toString() const;
    // 将还没读出的数据（二进制流）转为string（将二进制数据以十六进制表示）
    const std::string toHexString() const;

    // 重点：为了让ByteArray和socket更好的配合。将其转为iovec的结构(恰好Node很相似iovec)，可以直接sendmsg
    // 只读不改。注意这种方式的后续读取，position无法更新，故需在调用处手动更新
    uint64_t getReadBuffers(std::vector<iovec> &buffers, uint64_t len = ~0ULL) const;
    // 只读不改。从指定位置读取。注意这种方式的后续读取，position无法更新，故需在调用处手动更新
    uint64_t getReadBuffers(std::vector<iovec> &buffers, uint64_t len, size_t position) const;
    // socket获取到数据要向这里写入，提前告知要写入的数据大小，ByteArray增加容量，开辟好空间。注意这种方式的写入，position和size都无法更新，故需在调用处手动更新。设计的不好
    uint64_t getWriteBuffers(std::vector<iovec> &buffers, uint64_t len);
private:
    // 添加内存空间
    void addCapacity(size_t value);
    // 获取剩余的可用空间
    size_t getCapacity() const { return m_capacity - m_size; }

private:
    // 每个node占多大内存,单位为字节
    size_t m_baseSize;  
    // 记录当前读写位置,单位为字节
    size_t m_position;
    // 当前的真实大小,单位为字节
    size_t m_size;
    // 总共分配的内存,单位为字节
    size_t m_capacity;
    // 记录ByteArray存储数据的字节序。TODO：不确定是否需要？socket内部会帮数据做网络字节序转换吗？
    int8_t m_endian;
    // 链表的头节点
    Node *m_root;
    // 链表的当前节点
    Node *m_cur;
};

}

#endif