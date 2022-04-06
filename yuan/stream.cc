#include "stream.h"

namespace yuan {

int Stream::readFixSize(void *buffer, int length) {
    int left_len = length;
    int offset = 0;
    while (left_len > 0) {
        int len = read((char*)buffer + offset, left_len);
        if (len <= 0) {
            return len;
        }
        left_len -= len;
        offset += len;
    }
    return length;
}

int Stream::readFixSize(ByteArray::ptr ba, int length) {
    int left_len = length;
    while (left_len > 0) {
        int len = read(ba, left_len);
        if (len <= 0) {
            return len;
        }
        left_len -= len;
    }
    return length;
}

int Stream::writeFixSize(const void *buffer, int length) {
    int left_len = length;
    int offset = 0;
    while (left_len > 0) {
        int len = write((char*)buffer + offset, left_len);
        if (len <= 0) {
            return len;
        }
        left_len -= len;
        offset += len;
    }
    return length;
}

int Stream::writeFixSize(ByteArray::ptr ba, int length) {
    int left_len = length;
    while (left_len > 0) {
        int len = write(ba, left_len);
        if (len <= 0) {
            return len;
        }
        left_len -= len;
    }
    return length;
}

}