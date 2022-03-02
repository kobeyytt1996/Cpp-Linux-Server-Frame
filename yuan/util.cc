#include "util.h"

namespace yuan {

pid_t GetThreadId() {
     return syscall(SYS_gettid);
 }

// TODO:以后实现
uint32_t GetFiberId() {
    return 0;
}


}