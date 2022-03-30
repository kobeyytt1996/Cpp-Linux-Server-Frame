#ifndef __YUAN_BYTEARRAY_H__
#define __YUAN_BYTEARRAY_H__
/**
 * 该类用于网络传输的序列化。做网络协议的解析，方便取数据和统一写入数据
 * 比如收数据就是获得一块数据的内存，这个类可以让想读int就读int，想读string就读string
 */

#include <memory>
#include <stdint.h>
#include <string>

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
    void clear();

    void write(const void *buf, size_t size);
    void read(char *buf, size_t size);

    size_t getPosition() const { return m_position; }
    void setPosition(size_t val);

    // 方便的写入文件，可以用来当数据有问题时，做调试
    bool writeToFile(const std::string &name) const;
    // 从文件读出
    void readFromFile(const std::string &name);

    size_t getBaseSize() const { return m_baseSize; }
    // 获取还有多少数据可以读取
    size_t getReadSize() const { return m_size - m_position; }

    // 判断字节序
    bool isLittleEndian() const;
    // 设置字节序
    void setIsLittleEndian(bool is_little);
private:
    // 添加内存空间
    void addCapacity(size_t value);
    // 获取剩余的可用空间
    size_t getCapacity() const { return m_capacity - m_size; }

private:
    // 每个node占多大内存
    size_t m_baseSize;  
    // 记录当前读写位置
    size_t m_position;
    // 当前的真实大小
    size_t m_size;
    // 总共分配的内存
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