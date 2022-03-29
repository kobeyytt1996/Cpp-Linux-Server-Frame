#ifndef __YUAN_HOOK_H__
#define __YUAN_HOOK_H__

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/types.h>          
#include <sys/socket.h>

namespace yuan {
    bool is_hook_enable();
    // 可以决定哪些地方需要hook，哪些线程需要hook。会实现到线程级hook，线程内被hook的都会变为自己的实现
    void set_hook_enable(bool flag);
}

// C的编译规则里没有重载。而C++有，即编译器会通过不同的函数签名生成不同的函数名字来区分重载。
// 但这里我们指明使用C的编译规则，不使用重载：https://zhuanlan.zhihu.com/p/114669161
extern "C" {

/**
 * 以下三个和sleep相关。后续的类型和变量名字都使用固定的格式，方便hook.cc里使用宏
 */
typedef unsigned int (*sleep_fun)(unsigned int seconds);
// 用来保存原来的sleep实现，这样随时可以和新的切换
extern sleep_fun sleep_f;

typedef int (*usleep_fun)(useconds_t usec);
extern usleep_fun usleep_f;

typedef int (*nanosleep_fun)(const struct timespec *req, struct timespec *rem);
extern nanosleep_fun nanosleep_f;

/**
 * 以下的socket相关
 */
typedef int (*socket_fun)(int domain, int type, int protocol);
extern socket_fun socket_f;

typedef int (*connect_fun)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern connect_fun connect_f;

typedef int (*accept_fun)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern accept_fun accept_f;

/**
 * 以下的socket读相关
 */
typedef ssize_t (*read_fun)(int fd, void *buf, size_t count);
extern read_fun read_f;

typedef ssize_t (*readv_fun)(int fd, const struct iovec *iov, int iovcnt);
extern readv_fun readv_f;

typedef ssize_t (*recv_fun)(int sockfd, void *buf, size_t len, int flags);
extern recv_fun recv_f;

typedef ssize_t (*recvfrom_fun)(int sockfd, void *buf, size_t len, int flags,
                            struct sockaddr *src_addr, socklen_t *addrlen);
extern recvfrom_fun recvfrom_f;

typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr *msg, int flags);
extern recvmsg_fun recvmsg_f;

/**
 * 以下的socket的写相关
 */
typedef ssize_t (*write_fun)(int fd, const void *buf, size_t count);
extern write_fun write_f;

typedef ssize_t (*writev_fun)(int fd, const struct iovec *iov, int iovcnt);
extern writev_fun writev_f;

typedef ssize_t (*send_fun)(int sockfd, const void *buf, size_t len, int flags);
extern send_fun send_f;

typedef ssize_t (*sendto_fun)(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen);
extern sendto_fun sendto_f;

typedef ssize_t (*sendmsg_fun)(int sockfd, const struct msghdr *msg, int flags);
extern sendmsg_fun sendmsg_f;

// 关闭
typedef int (*close_fun)(int fd);
extern close_fun close_f;

/**
 * socket操作相关。控制非阻塞等
 */
typedef int (*fcntl_fun)(int fd, int cmd, ...);
extern fcntl_fun fcntl_f;

typedef int (*ioctl_fun)(int d, unsigned long int request, ...);
extern ioctl_fun ioctl_f;

typedef int (*getsockopt_fun)(int sockfd, int level, int optname,
                      void *optval, socklen_t *optlen);

extern getsockopt_fun getsockopt_f;

typedef int (*setsockopt_fun)(int sockfd, int level, int optname,
                      const void *optval, socklen_t optlen);
extern setsockopt_fun setsockopt_f;

// connect要实现的效果和do_io很像。但它有不同的设置超时时间的方法，故单独为其实现
extern int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms);

}

#endif