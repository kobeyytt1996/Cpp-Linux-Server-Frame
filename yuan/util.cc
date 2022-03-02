#include "util.h"
#include <stdint.h>

namespace yuan {

pid_t GetThreadId() {
     return syscall(SYS_gettid);
 }

// TODO:以后实现
u_int32_t GetFiberId() {
    return 0;
}


}