#include "config.h"
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

// 给tcp调用connect函约定一个超时时间的约定项，默认为5s
static yuan::ConfigVar<int>::ptr s_tcp_connect_timeout_config = 
    yuan::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");
// 只是存储上面约定项的值，配置文件修改了约定项，这里也跟着改变
static uint64_t s_connect_timeout = -1;
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

        s_connect_timeout = s_tcp_connect_timeout_config->getValue();
        s_tcp_connect_timeout_config->add_listener([](const int &old_value, const int &new_value){
            YUAN_LOG_INFO(g_system_logger) << "tcp connect timeout changed from "
                << old_value << " to " << new_value;
            s_connect_timeout = new_value;
        });
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
    // 标记是否已经取消了事件并保存设置的错误值
    int cancelled = 0;
};

// 重点：hook socket IO的统一实现方法。想要实现用法是同步的，但实际上是异步的效果。即有异步的高效性，又避免了使用异步时各种回调的复杂性。
// fun是要hook的系统函数，timeout_so是超时类型
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
    // 不是socket或用户设置过socket为非阻塞，则依旧走系统调用
    if (!fd_ctx->isSocket() || fd_ctx->getUserNonBlock()) {
        return fun(fd, std::forward<Args>(args)...);
    }
    // 从这里往后，hook IO的重要实现。fd一定是socket且用户没有设置过非阻塞（用户把它当作阻塞使用，虽然实际上是非阻塞的，但下面代码会让使用者用法和阻塞的相同）

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

// connect要实现的效果和do_io很像。但它有不同的设置超时时间的方法，故单独为其实现
int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms) {
    if (!yuan::t_hook_enable) {
        return connect_f(sockfd, addr, addrlen);
    }
    yuan::FdCtx::ptr fd_ctx = yuan::FdMgr::GetInstance()->get(sockfd);
    if (!fd_ctx || fd_ctx->isClosed()) {
        errno = EBADF;
        return -1;
    }
    if (!fd_ctx->isSocket() || fd_ctx->getUserNonBlock()) {
        return connect_f(sockfd, addr, addrlen);
    }

    int n = connect_f(sockfd, addr, addrlen);
    if (n == 0) {
        return 0;
    } else if (n != -1 || errno != EINPROGRESS) {
        // 看man connect了解EINPROGRESS
        return n;
    }

    yuan::IOManager *iomanager = yuan::IOManager::GetThis();
    yuan::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> wtinfo(tinfo);

    if (timeout_ms != static_cast<uint64_t>(-1)) {
        timer = iomanager->addConditionTimer(timeout_ms, [iomanager, sockfd, wtinfo](){
            auto t = wtinfo.lock();
            if (!t || t->cancelled) {
                return;
            }
            t->cancelled = ETIMEDOUT;
            iomanager->cancelEvent(sockfd, yuan::IOManager::WRITE);
        }, wtinfo);
    }

    // connect非阻塞时要监听写事件
    int ret = iomanager->addEvent(sockfd, yuan::IOManager::WRITE);
    if (ret == 0) {
        yuan::Fiber::YieldToHold();
        if (timer) {
            timer->cancel();
        }
        if (tinfo->cancelled) {
            errno = tinfo->cancelled;
            return -1;
        }
    } else {
        if (timer) {
            timer->cancel();
        }
        YUAN_LOG_ERROR(yuan::g_system_logger) << "connect addEvent(" << sockfd << ", WRITE) ERROR";
    }

    // sockfd上有写事件不代表connect成功，还需下面的判断。看man connect的EINPROGRESS部分
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        return -1;
    }
    if (!error) {
        return 0;
    } else {
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, yuan::s_connect_timeout);
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

// 实际只想hook cmd为F_SETFL和F_GETFL的设置阻塞与否的情况
int fcntl(int fd, int cmd, ...) {
    // 从va_list原理来看，并不能把可变参数传给下一个接收可变参数的函数，所以需要遍历cmd的所有情况。https://blog.csdn.net/aihao1984/article/details/5953668
    va_list va;
    va_start(va, cmd);
    switch (cmd) {
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                yuan::FdCtx::ptr fd_ctx = yuan::FdMgr::GetInstance()->get(fd);
                if (!fd_ctx || fd_ctx->isClosed() || !fd_ctx->isSocket()) {
                    return fcntl_f(fd, cmd, arg);
                }
                // 记录使用者是否设置了非阻塞。但实际的设置还是以系统的设置来进行
                fd_ctx->setUserNonBlock(arg & O_NONBLOCK);
                if (fd_ctx->getSysNonBlock()) {
                    arg |= O_NONBLOCK;
                } else {
                    arg &= (~O_NONBLOCK);
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int real_fl = fcntl_f(fd, cmd);
                yuan::FdCtx::ptr fd_ctx = yuan::FdMgr::GetInstance()->get(fd);
                if (!fd_ctx || fd_ctx->isClosed() || !fd_ctx->isSocket()) {
                    return real_fl;
                }
                if (fd_ctx->getUserNonBlock()) {
                    return real_fl | O_NONBLOCK;
                } else {
                    // 给使用者一种假象，他之前设置socket为阻塞的生效了
                    return real_fl & (~O_NONBLOCK);
                }
            }
            break;
        // 从man里依据可变参数的类型来区分各个cmd
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ:
            {
                int arg = va_arg(va, int);
                // va_end一定不能忘记调用：https://blog.csdn.net/jk110333/article/details/41940177?locationNum=8&fps=1
                va_end(va);
                return fcntl_f(fd, cmd, arg); 
            }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ:
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

// man ioctl可见可变参数只有一个且类型为void*。同fcntl，只需要处理设置阻塞与否相关的
// ioctl设置非阻塞：http://windfire-cd.github.io/blog/2012/11/13/linuxyi-bu-he-fei-zu-sai/
int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void *arg = va_arg(va, void*);
    va_end(va);

    if (request == FIONBIO) {
        yuan::FdCtx::ptr fd_ctx = yuan::FdMgr::GetInstance()->get(d);
        if (!fd_ctx || fd_ctx->isClosed() || !fd_ctx->isSocket()) {
            return ioctl_f(d, request, arg);
        }
        bool user_nonblock = !!*(static_cast<int*>(arg));
        fd_ctx->setUserNonBlock(user_nonblock);
        // 第三个参数1为阻塞，0为非阻塞
        if (fd_ctx->getSysNonBlock()) {
            *(static_cast<int*>(arg)) = 1;
        } else {
            *(static_cast<int*>(arg)) = 0;
        }
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname,
                      void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

// 只想hook用户设置socket阻塞时的阻塞超时时间的情况。https://www.cnblogs.com/fortunely/p/15055618.html
int setsockopt(int sockfd, int level, int optname,
                      const void *optval, socklen_t optlen) {
    if (!yuan::t_hook_enable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if (level == SOL_SOCKET) {
        if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            yuan::FdCtx::ptr fd_ctx = yuan::FdMgr::GetInstance()->get(sockfd);
            if (fd_ctx) {
                const timeval *tv = static_cast<const timeval*>(optval);
                // 将用户设置的值记录到FdCtx里。仅支持ms级别
                fd_ctx->setTimeout(optname, tv->tv_sec * 1000 + tv->tv_usec / 1000);
            }   
        }
    }
    // TODO:底层socket都设置为异步的，这里设置超时时间实际没什么影响?
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}
