#ifndef __FD_MANAGER_H__
#define __FD_MANAGER_H__
/**
 * @file fd_manager.h
 * 该头文件主要是辅助hook，记录fd的信息，区分出来socket
 * 
 */

#include <memory>
#include <vector>

#include "singleton.h"
#include "thread.h"

namespace yuan {

// fd的封装类，记录fd的信息。比如是否是socket，在hook掉的socket函数中需要用来判断
class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
    typedef std::shared_ptr<FdCtx> ptr;
    FdCtx(int fd);
    ~FdCtx();

    bool init();
    bool isInit() const { return m_isInit; }
    bool isSocket() const { return m_isSocket; }
    bool isClosed() const { return m_isClosed; }
    bool close();

    bool getSysNonBlock() const { return m_sysNonBlock; }
    void setSysNonBlock(bool flag);

    bool getUserNonBlock() const { return m_userNonBlock; }
    void setUserNonBlock(bool flag);

    uint64_t getTimeout(int type) const;
    void setTimeout(int type, uint64_t timeout);

private:
    bool m_isInit = false;
    bool m_isSocket = false;
    // 是否是系统设置的非阻塞
    bool m_sysNonBlock = false;
    // 是否是用户设置的非阻塞
    bool m_userNonBlock = false;
    bool m_isClosed = false;
    int m_fd;
    // 记录socket收发数据的超时时间
    uint64_t m_recvTimeout = 0;
    uint64_t m_sendTimeout = 0;

};

// fd管理类，使用时要用下面的单例模式
class FdManager {
public:
    typedef RWMutex RWMutexType;
    FdManager();

    // auto_create为true，则fd封装类不存在就创建一个
    FdCtx::ptr get(int fd, bool auto_create = false);

    void del(int fd);

private:
    RWMutexType m_mutex;
    // 下标代表fd，空间换时间
    std::vector<FdCtx::ptr> m_datas;
};

typedef Singleton<FdManager> FdMgr;

}

#endif