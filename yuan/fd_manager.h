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
    // 只有socket会使用hook后的IO实现
    bool m_isSocket = false;
    // 记录系统是否设置了非阻塞。（通过系统hook创建的socket都应该是非阻塞）e.g. 构造函数中修改了这个值
    bool m_sysNonBlock = false;
    // 使用者是否是使用了fcntl等方式设置了fd为阻塞。hook掉这些方式来修改这个值记录用户的设置。
    // 如果用户设置为了非阻塞，说明用户不想使用这一套非阻塞模拟阻塞的机制，则hook IO实现时直接用系统函数
    bool m_userNonBlock = false;
    bool m_isClosed = false;
    int m_fd;
    // 记录用户设置socket阻塞时设置的收发阻塞超时时间。仅支持ms级别
    // hook用户设置这两个时间的函数，将设置值记录到这里。用iomanager的定时器功能实现出看起来一样的效果。
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
    // 记录了所有的想要记录管理的fd。下标代表fd，空间换时间
    std::vector<FdCtx::ptr> m_datas;
};

typedef Singleton<FdManager> FdMgr;

}

#endif