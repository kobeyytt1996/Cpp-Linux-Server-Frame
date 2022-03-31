#include "../yuan/bytearray.h"
#include "../yuan/yuan_all_headers.h"

static yuan::Logger::ptr g_logger = YUAN_GET_ROOT_LOGGER();

void test() {
#define XX(type, len, write_fun, read_fun, base_len) {\
    std::vector<type> vec; \
    for (int i = 0; i < len; ++i) { \
        vec.push_back(rand()); \
    } \
    yuan::ByteArray::ptr ba(new yuan::ByteArray(base_len)); \
    for (auto i : vec) { \
        ba->write_fun(i); \
    } \
    ba->setPosition(0); \
    for (size_t i = 0; i < vec.size(); ++i) { \
        type v = ba->read_fun(); \
        YUAN_ASSERT(v == vec[i]); \
    } \
    YUAN_ASSERT(ba->getReadSize() == 0); \
    YUAN_LOG_INFO(g_logger) << #write_fun "/" #read_fun \
        << " (" #type ") len=" << len << " base_len=" << base_len \
        << " size=" << ba->getSize(); \
}
    // XX(int8_t, 100, writeFint8, readFint8, 10);
    // XX(uint8_t, 100, writeFuint8, readFuint8, 10);
    // XX(int16_t, 100, writeFint16, readFint16, 10);
    // XX(uint16_t, 100, writeFuint16, readFuint16, 10);
    // XX(int32_t, 100, writeFint32, readFint32, 10);
    // XX(uint32_t, 100, writeFuint32, readFuint32, 10);
    // XX(int64_t, 100, writeFint64, readFint64, 10);
    // XX(uint64_t, 100, writeFuint64, readFuint64, 10);

    // XX(int32_t, 100, writeInt32, readInt32, 10);
    // XX(uint32_t, 100, writeUint32, readUint32, 10);
    // XX(int64_t, 100, writeInt64, readInt64, 10);
    // XX(uint64_t, 100, writeUint64, readUint64, 10);
#undef XX

#define XX(type, len, write_fun, read_fun, base_len) {\
    std::vector<type> vec; \
    for (int i = 0; i < len; ++i) { \
        vec.push_back(rand()); \
    } \
    yuan::ByteArray::ptr ba(new yuan::ByteArray(base_len)); \
    for (auto i : vec) { \
        ba->write_fun(i); \
    } \
    ba->setPosition(0); \
    for (size_t i = 0; i < vec.size(); ++i) { \
        type v = ba->read_fun(); \
        YUAN_ASSERT(v == vec[i]); \
    } \
    YUAN_ASSERT(ba->getReadSize() == 0); \
    YUAN_LOG_INFO(g_logger) << #write_fun "/" #read_fun \
        << " (" #type ") len=" << len << " base_len=" << base_len \
        << " size=" << ba->getSize(); \
    ba->setPosition(0); \
    YUAN_ASSERT(ba->writeToFile("/tmp/" #type "_" #len ".dat")); \
    yuan::ByteArray::ptr ba2(new yuan::ByteArray(base_len * 2)); \
    YUAN_ASSERT(ba2->readFromFile("/tmp/" #type "_" #len ".dat")); \
    ba2->setPosition(0); \
    YUAN_ASSERT(ba->toString() == ba2->toString()); \
    YUAN_ASSERT(ba->getPosition() == 0); \
    YUAN_ASSERT(ba2->getPosition() == 0); \
}
    XX(int8_t, 2, writeFint8, readFint8, 1);
    XX(uint8_t, 100, writeFuint8, readFuint8, 1);
    XX(int16_t, 100, writeFint16, readFint16, 1);
    XX(uint16_t, 100, writeFuint16, readFuint16, 1);
    XX(int32_t, 100, writeFint32, readFint32, 1);
    XX(uint32_t, 2, writeFuint32, readFuint32, 1);
    XX(int64_t, 100, writeFint64, readFint64, 1);
    XX(uint64_t, 100, writeFuint64, readFuint64, 1);

    XX(int32_t, 100, writeInt32, readInt32, 1);
    XX(uint32_t, 100, writeUint32, readUint32, 1);
    XX(int64_t, 100, writeInt64, readInt64, 1);
    XX(uint64_t, 100, writeUint64, readUint64, 1);

#undef XX

}



int main(int argc, char **argv) {
    test();
    return 0;
}