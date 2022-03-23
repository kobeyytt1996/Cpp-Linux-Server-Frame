#include "fd_manager.h"
#include "fiber.h"
#include "hook.h"
#include "iomanager.h"
#include "log.h"

// 编译的时候要加上链接库：dl
#include <dlfcn.h>
#include <functional>

namespace yuan {

static yuan::Logger::ptr g_system_logger = YUAN_GET_LOGGER("system");

// hook是以线程为单位，故使用thread_local
static thread_local bool t_hook_enable = false;

// 要hook的函数列表
#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt) 

// 将我们声明的变量指针指向要hook的系统的原函数（使用dlsym从动态库里取出的）
void hook_init() {
    static bool is_inited = false;
    if (is_inited) {
        return;
    }
// 重点：##表示字符串连接。dlsym:https://blog.csdn.net/mijichui2153/article/details/109561978。或看man
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX)
#undef XX
}

// 要在main函数之前初始化hook。利用数据初始化的顺序，如全局变量在main进入之前就初始化。把要做的放在构造方法里。
struct _HookIniter {
    _HookIniter() {
        hook_init();
    }
};

static _HookIniter s_hook_initer;

bool is_hook_enable() {
    return t_hook_enable;
}

void set_hook_enable(bool flag) {
    t_hook_enable = flag;
}

}

// 超时条件类
struct timer_info {
    // 标记是否已经取消了事件并设置过错误
    int cancelled = 0;
};

// 重点：hook socket IO的统一实现方法。想要实现用起来是同步的，但实际上是异步的效果。fun是要hook的系统函数，timeout_so是超时类型
template<typename OriginFun, typename ... Args>
static ssize_t do_io(int fd, OriginFun fun, const char *hook_fun_name, uint32_t event
                , int timeout_so, Args &&... args) {
    if (!yuan::t_hook_enable) {
        return fun(fd, std::forward<Args>(args)...);
    }

    yuan::FdCtx::ptr fd_ctx = yuan::FdMgr::GetInstance()->get(fd);
    if (!fd_ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }

    if (fd_ctx->isClosed()) {
        // 看man 2 read，了解EBADF
        errno = EBADF;
        return -1;
    }

    if (!fd_ctx->isSocket() || fd_ctx->getUserNonBlock()) {
        return fun(fd, std::forward<Args>(args)...);
    }
    // 从这里往后，hook IO的重要实现。fd一定是socket且没设置用户级别的非阻塞

    uint64_t timeout = fd_ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while (n == -1 && errno == EINTR) {
        n = fun(fd, std::forward<Args>(args)...);
    }
    // 看man 2 read了解。说明设置了非阻塞且这次已读完或已写满。此时需要异步操作
    if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        yuan::IOManager *iomanager = yuan::IOManager::GetThis();
        yuan::Timer::ptr timer;
        std::weak_ptr<timer_info> wtinfo(tinfo);

        // 有设置超时时间
        if (timeout != static_cast<uint64_t>(-1)) {
            timer = iomanager->addConditionTimer(timeout, [wtinfo, fd, iomanager, event](){
                auto t = wtinfo.lock();
                if (!t || t->cancelled) {
                    return;
                }
                // man 2 connect可以看到该错误
                t->cancelled = ETIMEDOUT;
                // 已经超时，取消epoll监听事件，并强制唤醒当前协程。（因为下面addEvent时没加回调的实参）
                iomanager->cancelEvent(fd, static_cast<yuan::IOManager::Event>(event));
            }, wtinfo);
        }
        // 计数器，记录重试了多少次
        // int count = 0;
        // 记录开始的时间
        // uint64_t start_time = 0;

        // 在epoll里继续监听该fd上的该事件。参数没加回调，则当前协程为唤醒对象。返回值不为0，则添加监听失败
        int ret = iomanager->addEvent(fd, static_cast<yuan::IOManager::Event>(event));
        if (ret) {
            YUAN_LOG_ERROR(yuan::g_system_logger) << hook_fun_name << "addEvent("
                << fd << " , " << event << ")";
            if (timer) {
                // 监听事件失败，则定时器也取消掉
                timer->cancel();
            }
            return -1;
        }
        // epoll添加事件监听成功 
        else {
            // YieldToHold和YieldToReady的区别主要看scheduler.cc里的run方法
            yuan::Fiber::YieldToHold();
            // 协程被唤醒，可能从两个点唤醒回来：监听的事件及时发生，或超时导致上面的cancelEvent被调用到。
            // 无论哪种情况，有定时器就要cancel掉
            if (timer) {
                timer->cancel();
            }
            // 判断是因为哪种情况被唤醒的。如果已经超时，则不再尝试读写数据，直接返回
            if (tinfo->cancelled) {
                errno = tinfo->cancelled;
                return -1;
            }
            // 再次尝试调用系统的IO。goto的坏处是跳跃可能导致某些数据结构没有初始化。但这里不存在该问题且只是简单使用
            goto retry;
        }
    }
    // 成功读到数据，返回
    return n;
}

// TODO: 经测试，这里如果不加extern "C"，生成的执行程序的符号表里sleep依旧是按C编译规则命名。但如果#include <unistd.h>也去掉，则sleep按C++规则命名。不确定这里的原因？
extern "C" {

// 定义所有要hook的函数指针
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX)
#undef XX

// hook sleep的实现。即实现sleep的定义，让编译器把<unistd.h>的sleep声明和这里链接起来，而不是链接动态库里的sleep实现
// 原系统的sleep会让整个线程sleep，这里只是让fiber sleep。表现的现象和系统的一样，但不阻塞线程。大大提高效率。
unsigned int sleep(unsigned int seconds) {
    if (!yuan::t_hook_enable) {
        // 不hook则使用系统的原函数
        return sleep_f(seconds);
    }

    yuan::Fiber::ptr fiber = yuan::Fiber::GetThis();
    yuan::IOManager *iomanager = yuan::IOManager::GetThis();
    // sleep被hook为，当前协程放弃执行权，添加定时器，到了指定时间再被放到任务队列里被执行
    // 注意下面bind的使用、加转型是因为Scheduler里有两个schedule的实现，不强转就不知道要bind哪一个。另外有默认值的参数这里也要明确赋值，不能像函数调用一样
    iomanager->addTimer(seconds * 1000
        , std::bind(static_cast<void(yuan::Scheduler::*)(yuan::Fiber::ptr, int thread)>(&yuan::Scheduler::schedule)
            , iomanager, fiber, -1));
    // iomanager->addTimer(seconds * 1000, [iomanager, fiber](){
    //     iomanager->schedule(fiber);
    // });
    yuan::Fiber::YieldToHold();

    return 0;
}

// 与sleep的实现一致，这里的时间单位是微妙
int usleep(useconds_t usec) {
    if (!yuan::t_hook_enable) {
        // 不hook则使用系统的原函数
        return usleep_f(usec);
    }

    yuan::Fiber::ptr fiber = yuan::Fiber::GetThis();
    yuan::IOManager *iomanager = yuan::IOManager::GetThis();
    // usleep被hook为，当前协程放弃执行权，添加定时器，到了指定时间再被放到任务队列里被执行
    iomanager->addTimer(usec / 1000, 
        std::bind(static_cast<void(yuan::Scheduler::*)(yuan::Fiber::ptr, int thread)>(&yuan::Scheduler::schedule)
            , iomanager, fiber, -1));
    // iomanager->addTimer(usec / 1000, [iomanager, fiber](){
    //     iomanager->schedule(fiber);
    // });
    yuan::Fiber::YieldToHold();

    return 0;
}

// 与sleep的实现一致
int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!yuan::t_hook_enable) {
        // 不hook则使用系统的原函数
        return nanosleep_f(req, rem);
    }

    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    yuan::Fiber::ptr fiber = yuan::Fiber::GetThis();
    yuan::IOManager *iomanager = yuan::IOManager::GetThis();
    iomanager->addTimer(timeout_ms, 
        std::bind(static_cast<void(yuan::Scheduler::*)(yuan::Fiber::ptr, int thread)>(&yuan::Scheduler::schedule)
            , iomanager, fiber, -1));
    yuan::Fiber::YieldToHold();

    return 0;
}

int socket(int domain, int type, int protocol) {
    if (!yuan::t_hook_enable) {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if (fd == -1) {
        return fd;
    }
    // 关键是在fd管理类类加入这个fd
    yuan::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    // TODO:暂时先不实现
    return connect_f(sockfd, addr, addrlen);
}


int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int fd = do_io(sockfd, accept_f, "accept", yuan::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if (fd >= 0) {
        // 和上面socket一样，也是要创建socket fd。只不过是不同的socket
        yuan::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, read_f, "read", yuan::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", yuan::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", yuan::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", yuan::IOManager::READ, SO_RCVTIMEO
        , buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", yuan::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, write_f, "write", yuan::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", yuan::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return do_io(sockfd, send_f, "send", yuan::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen) {
    return do_io(sockfd, sendto_f, "sendto", yuan::IOManager::WRITE, SO_SNDTIMEO
                , buf, len, flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    return do_io(sockfd, sendmsg_f, "sendmsg", yuan::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
    if (!yuan::t_hook_enable) {
        return close_f(fd);
    }

    yuan::FdCtx::ptr fd_ctx = yuan::FdMgr::GetInstance()->get(fd);
    if (fd_ctx) {
        auto iomanager = yuan::IOManager::GetThis();
        if (iomanager) {
            iomanager->cancelAll(fd);
        }
        yuan::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, ...) {
    
}

int ioctl(int d, int request, ...) {

}

int getsockopt(int sockfd, int level, int optname,
                      void *optval, socklen_t *optlen) {
    
}

int setsockopt(int sockfd, int level, int optname,
                      const void *optval, socklen_t optlen) {
    
}

}
