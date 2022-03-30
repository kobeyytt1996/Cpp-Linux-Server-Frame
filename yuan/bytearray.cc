#include <string.h>

#include "bytearray.h"
#include "endian.h"

namespace yuan {

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

static uint64_t EncodeZigzag64(int64_t value) {
    if (value < 0) {
        // TODO:这里对于INT32_MIN存在问题
        return ((uint64_t)(-value)) * 2 - 1;
    } else {
        return value * 2;
    }
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

int8_t ByteArray::readFint8();
uint8_t ByteArray::readFuint8();
int16_t ByteArray::readFint16();
uint16_t ByteArray::readFuint16();
int32_t ByteArray::readFint32();
uint32_t ByteArray::readFuint32();
int64_t ByteArray::readFint64();
uint64_t ByteArray::readFuint64();

int32_t ByteArray::readInt32();
uint32_t ByteArray::readUint32();
int64_t ByteArray::readInt64();
uint64_t ByteArray::readUint64();

float ByteArray::readFloat();
double ByteArray::readDouble();

std::string ByteArray::readStringF16();
std::string ByteArray::readStringF32();
std::string ByteArray::readStringF64();
std::string ByteArray::readStringVint();

void ByteArray::clear();
void ByteArray::write(const void *buf, size_t size);
void ByteArray::read(char *buf, size_t size);
void ByteArray::setPosition(size_t val);
bool ByteArray::writeToFile(const std::string &name) const;
void ByteArray::readFromFile(const std::string &name);
void ByteArray::addCapacity(size_t value);

bool ByteArray::isLittleEndian() const {
    return m_endian == YUAN_LITTLE_ENDIAN;
}

void ByteArray::setIsLittleEndian(bool is_little) {
    m_endian = is_little ? YUAN_LITTLE_ENDIAN : YUAN_BIG_ENDIAN;
}

}