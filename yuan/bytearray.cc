#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string.h>

#include "bytearray.h"
#include "endian.h"
#include "log.h"

namespace yuan {

static Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

/**
 * 自定义的单链表Node的函数实现
 */
ByteArray::Node::Node(size_t s)
    : ptr(new char[s])
    , size(s)
    , next(nullptr) {}

ByteArray::Node::Node()
    : ptr(nullptr)
    , size(0)
    , next(nullptr) {}

ByteArray::Node::~Node() {
    if (ptr) {
        delete [] ptr;
    }
}

/**
 * ByteArray的所有方法实现
 */
ByteArray::ByteArray(size_t base_size)
    : m_baseSize(base_size)
    , m_position(0)
    , m_size(0)
    , m_capacity(base_size)
// 网络字节序默认是大端
    , m_endian(YUAN_BIG_ENDIAN)
    , m_root(new Node(base_size))
    , m_cur(m_root) {}

ByteArray::~ByteArray() {
    Node *temp = m_root;
    while (temp) {
        m_cur = temp;
        temp = temp->next;
        delete m_cur;
    }
}

void ByteArray::writeFint8(int8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFuint8(uint8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFint16(int16_t value) {
    // ByteArray需要的字节序和本地的不一样，需要做转换
    if (m_endian != YUAN_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFuint16(uint16_t value) {
    if (m_endian != YUAN_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFint32(int32_t value) {
    if (m_endian != YUAN_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFuint32(uint32_t value) {
    if (m_endian != YUAN_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFint64(int64_t value) {
    if (m_endian != YUAN_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

void ByteArray::writeFuint64(uint64_t value) {
    if (m_endian != YUAN_BYTE_ORDER) {
        value = byteswap(value);
    }
    write(&value, sizeof(value));
}

// 按照google压缩算法，负数压缩效果不好。所以转成正数。为和正数区分，则用奇偶区分。如1转为2，-1转为1
static uint32_t EncodeZigzag32(int32_t value) {
    if (value < 0) {
        // TODO:这里对于INT32_MIN存在问题
        return ((uint32_t)(-value)) * 2 - 1;
    } else {
        return value * 2;
    }
}

// 上面编码的解码
static int32_t DecodeZigzag32(uint32_t value) {
    // 重点：下面写法非常巧妙，负号即取补码。如果value为奇数，-(value & 1)即所有位都为1
    return (value >> 1) ^ -(value & 1);
}

static uint64_t EncodeZigzag64(int64_t value) {
    if (value < 0) {
        // TODO:这里对于INT64_MIN存在问题
        return ((uint64_t)(-value)) * 2 - 1;
    } else {
        return value * 2;
    }
}

static int64_t DecodeZigzag64(uint64_t value) {
    return (value >> 1) ^ -(value & 1);
}

void ByteArray::writeInt32(int32_t value) {
    uint32_t tmp = EncodeZigzag32(value);
    writeUint32(tmp);
}

void ByteArray::writeUint32(uint32_t value) {
    // 最极端情况，压缩后增加到5个字节
    uint8_t tmp[5];
    uint8_t i = 0;
    // 使用谷歌的压缩算法。看头文件里的注释
    while (value >= 0x80) {
        tmp[i++] = (value & 0x7f) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void ByteArray::writeInt64(int64_t value) {
    writeUint64(EncodeZigzag64(value));
}

void ByteArray::writeUint64(uint64_t value) {
    uint8_t tmp[10];
    uint8_t i = 0;
    while (value >= 0x80) {
        tmp[i++] = (value & 0x7f) | 0x80;
        value >>= 7;
    }
    tmp[i++] = value;
    write(tmp, i);
}

void ByteArray::writeFloat(float value) {
    // TODO:理论上将浮点型应该不需要转字节序，因为实际上当作数组存储:https://zhuanlan.zhihu.com/p/45874116
    // https://blog.csdn.net/yangshuanbao/article/details/6913623
    // 但这里保险起见，转成整形，再按整形write的方式
    uint32_t tmp;
    memcpy(&tmp, &value, sizeof(value));
    writeFuint32(tmp);
}

void ByteArray::writeDouble(double value) {
    uint64_t tmp;
    memcpy(&tmp, &value, sizeof(value));
    writeFuint64(tmp);
}

void ByteArray::writeStringF16(const std::string &value) {
    writeFuint16(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringF32(const std::string &value) {
    writeFuint32(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringF64(const std::string &value) {
    writeFuint64(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringVint(const std::string &value) {
    writeUint64(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringWithoutLength(const std::string &value) {
    write(value.c_str(), value.size());
}

int8_t ByteArray::readFint8() {
    int8_t value;
    read(&value, sizeof(value));
    return value;
}

uint8_t ByteArray::readFuint8() {
    uint8_t value;
    read(&value, sizeof(value));
    return value;
}

#define READ(type) \
    type value; \
    read(&value, sizeof(value)); \
    if (YUAN_BYTE_ORDER != m_endian) { \
        value = byteswap(value); \
    } \
    return value; 

int16_t ByteArray::readFint16() {
    READ(int16_t);
}

uint16_t ByteArray::readFuint16() {
    READ(uint16_t);
}

int32_t ByteArray::readFint32() {
    READ(int32_t);
}

uint32_t ByteArray::readFuint32() {
    READ(uint32_t);
}

int64_t ByteArray::readFint64() {
    READ(int64_t);
}

uint64_t ByteArray::readFuint64() {
    READ(uint64_t);
}

#undef READ

int32_t ByteArray::readInt32() {
    return DecodeZigzag32(readUint32());
}

uint32_t ByteArray::readUint32() {
    uint32_t result = 0;
    for (int i = 0; i < 32; i += 7) {
        uint8_t tmp = readFuint8();
        if (tmp < 0x80) {
            result |= (((uint32_t)tmp) << i);
            break;
        } else {
            result |= (((uint32_t)(tmp & 0x7f)) << i);
        }
    }
    return result;
}

int64_t ByteArray::readInt64() {
    return DecodeZigzag64(readUint64());
}

uint64_t ByteArray::readUint64() {
    uint64_t result = 0;
    for (int i = 0; i < 64; i += 7) {
        uint8_t tmp = readFuint8();
        if (tmp < 0x80) {
            result |= (((uint64_t)tmp) << i);
            break;
        } else {
            result |= (((uint64_t)(tmp & 0x7f)) << i);
        }
    }
    return result;
}

float ByteArray::readFloat() {
    uint32_t v = readFuint32();
    float value;
    memcpy(&value, &v, sizeof(v));
    return value;
}

double ByteArray::readDouble() {
    uint64_t v = readFuint64();
    double value;
    memcpy(&value, &v, sizeof(v));
    return value;
}

std::string ByteArray::readStringF16() {
    uint16_t len = readFuint16();
    std::string buff(len, '\0');
    read(&buff[0], len);
    return buff;
}

std::string ByteArray::readStringF32() {
    uint32_t len = readFuint32();
    std::string buff(len, '\0');
    read(&buff[0], len);
    return buff;
}

std::string ByteArray::readStringF64() {
    uint64_t len = readFuint64();
    std::string buff(len, '\0');
    read(&buff[0], len);
    return buff;
}

std::string ByteArray::readStringVint() {
    uint64_t len = readFuint64();
    std::string buff(len, '\0');
    read(&buff[0], len);
    return buff;
}

void ByteArray::clear() {
    m_position = m_size = 0;
    m_capacity = m_baseSize;

    m_cur = m_root->next;
    Node *temp;
    while (m_cur) {
        temp = m_cur;
        m_cur = m_cur->next;
        delete temp;
    }
    m_cur = m_root;
    m_root->next = nullptr;
}

void ByteArray::write(const void *buf, size_t size) {
    if (size == 0) {
        return;
    }

    addCapacity(size);
    size_t node_pos = m_position % m_baseSize;
    size_t node_cap = m_cur->size - node_pos;
    size_t buf_pos = 0;

    while (size > 0) {
        if (size <= node_cap) {
            memcpy(m_cur->ptr + node_pos, (const char *)buf + buf_pos, size);
            if (node_cap == size) {
                m_cur = m_cur->next;
            }
            m_position += size;
            size = 0;
        } else {
            memcpy(m_cur->ptr + node_pos, (const char *)buf + buf_pos, node_cap);
            m_cur = m_cur->next;
            m_position += node_cap;
            size -= node_cap;
            buf_pos += node_cap;
            node_pos = 0;
            node_cap = m_cur->size;
        }
    }

    if (m_position > m_size) {
        m_size = m_position;
    }
}

void ByteArray::read(void *buf, size_t size) {
    if (size > getReadSize()) {
        throw std::out_of_range("not enough len");
    }

    size_t node_pos = m_position % m_cur->size;
    size_t node_cap = m_cur->size - node_pos;
    size_t buf_pos = 0;

    while (size > 0) {
        if (size <= node_cap) {
            memcpy((char *)buf + buf_pos, m_cur->ptr + node_pos, size);
            if (size == node_cap) {
                m_cur = m_cur->next;
            }
            m_position += size;
            size = 0;
        } else {
            memcpy((char *)buf + buf_pos, m_cur->ptr + node_pos, node_cap);
            m_cur = m_cur->next;
            m_position += node_cap;
            size -= node_cap;
            node_pos = 0;
            buf_pos += node_cap;
            node_cap = m_cur->size;
        }
    }
}

void ByteArray::read(void *buf, size_t size, size_t position) const {
    size_t read_size = m_size - position;
    if (size > read_size) {
        throw std::out_of_range("not enough len");
    }

    size_t node_pos = position % m_cur->size;
    size_t node_cap = m_cur->size - node_pos;
    size_t buf_pos = 0;
    Node *cur = m_cur;

    while (size > 0) {
        if (size <= node_cap) {
            memcpy((char *)buf + buf_pos, cur->ptr + node_pos, size);
            if (size == node_cap) {
                cur = cur->next;
            }
            position += size;
            size = 0;
        } else {
            memcpy((char *)buf + buf_pos, cur->ptr + node_pos, node_cap);
            cur = cur->next;
            position += node_cap;
            size -= node_cap;
            node_pos = 0;
            buf_pos += node_cap;
            node_cap = cur->size;
        }
    }
}

void ByteArray::setPosition(size_t val) {
    if (val > m_capacity) {
        throw std::out_of_range("setPosition out of capacity");
    }

    // 如果指定位置大于size，则增加size。这里设计的不好，但为了一些情况下增加数据但无法改size准备的。比如调用getWriteBuffers之后
    if (val > m_size) {
        m_size = val;
    }
    m_position = val;
    m_cur = m_root;
    // 注意val是m_cur->size整数倍的边界情况，m_cur易空指针
    while (val > m_cur->size) {
        val -= m_cur->size;
        m_cur = m_cur->next;
    }
    if (val == m_cur->size) {
        m_cur = m_cur->next;
    }
}

bool ByteArray::writeToFile(const std::string &name) const {
    // 注意：要以二进制流的方式打开。常用的<<和>>都是基于字符的，不能再用了，要用write：https://gcc.gnu.org/onlinedocs/libstdc++/manual/fstreams.html#std.io.filestreams.binary
    std::ofstream ofs(name, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs) {
        YUAN_LOG_ERROR(g_system_logger) << "writeToFile name=" << name << " open failed"
            << " errno=" << errno << " strerr=" << strerror(errno);
        return false;
    }

    size_t read_size = getReadSize();
    size_t pos = m_position;
    Node *cur = m_cur;

    while (read_size > 0) {
        size_t node_pos = pos % m_baseSize;
        size_t len = (read_size > m_baseSize ? m_baseSize : read_size) - node_pos;
        // 二进制要用write，是unformatted output function
        ofs.write(cur->ptr + node_pos, len);
        cur = cur->next;
        pos += len;
        read_size -= len;
    }

    return true;
}

bool ByteArray::readFromFile(const std::string &name) {
    std::ifstream ifs(name, std::ios::binary | std::ios::in);
    if (!ifs) {
        YUAN_LOG_ERROR(g_system_logger) << "readFromFile name=" << name << " open failed"
            << " errno=" << errno << " strerr=" << strerror(errno);
        return false;
    }

    // 智能指针管理new出的数组，要加deleter
    std::shared_ptr<char> buff(new char[m_baseSize], [](char *ptr){ delete[] ptr; });
    while (ifs) {
        ifs.read(buff.get(), m_baseSize);
        // 注意io流和二进制数据相关的计数API
        this->write(buff.get(), ifs.gcount());
    }
    return true;
}

void ByteArray::addCapacity(size_t value) {
    if (value == 0) {
        return;
    }

    size_t old_cap = getCapacity();
    if (value <= old_cap) {
        return;
    }

    value -= old_cap;
    size_t new_nodes = value / m_baseSize + (value % m_baseSize ? 1 : 0);

    Node *tmp = m_root;
    while (tmp->next) {
        tmp = tmp->next;
    }

    Node *first_new_node = nullptr;
    while (new_nodes > 0) {
        tmp->next = new Node(m_baseSize);
        tmp = tmp->next;
        if (!first_new_node) {
            first_new_node = tmp;
        }
        --new_nodes;
        m_capacity += m_baseSize;
    }

    // 在前面read、write等操作中，m_cur可能指向空，一定要确保m_cur不指向空
    if (!m_cur) {
        m_cur = first_new_node;
    }
}

bool ByteArray::isLittleEndian() const {
    return m_endian == YUAN_LITTLE_ENDIAN;
}

void ByteArray::setIsLittleEndian(bool is_little) {
    m_endian = is_little ? YUAN_LITTLE_ENDIAN : YUAN_BIG_ENDIAN;
}

const std::string ByteArray::toString() const {
    std::string str;
    str.resize(getReadSize());
    if (str.empty()) {
        return str;
    }

    read(&str[0], str.size(), m_position);
    return str;
}

const std::string ByteArray::toHexString() const {
    std::string str = toString();
    std::stringstream ss;

    for (size_t i = 0; i < str.size(); ++i) {
        // 每32个字节输出一个空格
        if (i > 0 && i % 32 == 0) {
            ss << std::endl;
        }
        // 注意如何使用这些格式化操作符
        ss << std::setw(2) << std::setfill('0') << std::hex 
            << (int)(uint8_t)str[i] << " " << std::dec;
    }

    return ss.str();
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec> &buffers, uint64_t len) const  {
    len = len > getReadSize() ? getReadSize() : len;
    if (len == 0) {
        return 0;
    }

    size_t node_pos = m_position % m_baseSize;
    size_t node_cap = m_cur->size - node_pos;
    Node *cur = m_cur;
    iovec iov;

    uint64_t size = len;

    while (len > 0) {
        if (node_cap >= len) {
            iov.iov_base = cur->ptr + node_pos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + node_pos;
            iov.iov_len = node_cap;
            len -= node_pos;
            cur = cur->next;
            node_pos = 0;
            node_cap = cur->size;
        }
        buffers.push_back(iov);
    }

    return size;
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec> &buffers, uint64_t len, size_t position) const {
    size_t read_size = m_size - position;
    len = len > read_size ? read_size : len;
    if (len == 0) {
        return 0;
    }

    size_t node_count = position / m_baseSize;
    Node *cur = m_root;
    while (node_count > 0) {
        cur = cur->next;
        --node_count;
    }

    size_t node_pos = position % m_baseSize;
    size_t node_cap = cur->size - node_pos;
    iovec iov;

    uint64_t size = len;

    while (len > 0) {
        if (node_cap >= len) {
            iov.iov_base = cur->ptr + node_pos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + node_pos;
            iov.iov_len = node_cap;
            len -= node_pos;
            cur = cur->next;
            node_pos = 0;
            node_cap = cur->size;
        }
        buffers.push_back(iov);
    }

    return size;
}

uint64_t ByteArray::getWriteBuffers(std::vector<iovec> &buffers, uint64_t len) {
    if (len == 0) {
        return 0;
    }
    // 和上面的关键区别在这里，要扩容
    addCapacity(len);

    size_t node_pos = m_position % m_baseSize;
    size_t node_cap = m_cur->size - node_pos;
    Node *cur = m_cur;
    iovec iov;

    uint64_t size = len;

    while (len > 0) {
        if (node_cap >= len) {
            iov.iov_base = cur->ptr + node_pos;
            iov.iov_len = len;
            len = 0;
        } else {
            iov.iov_base = cur->ptr + node_pos;
            iov.iov_len = node_cap;
            len -= node_pos;
            cur = cur->next;
            node_pos = 0;
            node_cap = cur->size;
        }
        buffers.push_back(iov);
    }

    return size;
}

}