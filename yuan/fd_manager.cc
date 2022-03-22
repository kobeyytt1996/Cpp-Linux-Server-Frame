#include "fd_manager.h"

namespace yuan {

/**
 * FdCtx的方法实现
 */
FdCtx::FdCtx(int fd) 
    : m_isInit(false) 
    , m_isSocket(false) 
    , m_sysNonBlock(false)
    , m_userNonBlock(false)
    , m_isClosed(false)
    , m_fd(fd)
    , m_recvTimeout(-1)
    , m_sendTimeout(-1) {
        init();
}

FdCtx::~FdCtx() {
}

bool FdCtx::init() {
    if (m_isInit) {
        return true;
    }
    m_recvTimeout = -1;
    m_sendTimeout = -1;

    
}

bool FdCtx::close() {

}

void FdCtx::setSysNonBlock(bool flag) {

}

void FdCtx::setUserNonBlock(bool flag) {

}

void FdCtx::setTimeout(int type, uint64_t timeout) {

}

/**
 * FdManager的方法实现
 */
FdManager::FdManager() {
    m_datas.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create = false) {

}

void FdManager::del(int fd) {

}
}