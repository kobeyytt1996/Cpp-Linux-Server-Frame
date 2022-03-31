# 调用make clean
cmake_minimum_required(VERSION 2.8)
project(yuan)

# 可以使用该文件里的一个函数，编译的时候能自定义宏。目的是为了修改__FILE__，让日志输出的时候不要打印绝对路径而是相对路径。这样不会暴露绝对路径
include (cmake/utils.cmake)

# make过程中显示详细命令
set(CMAKE_VERBOSE_MAKEFILE ON)
# 自定义的编译参数 http://blog.sina.com.cn/s/blog_553230d70101efqv.html
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-error=builtin-macro-redefined")

include_directories(.)
include_directories(/usr/local/boost/include/)
include_directories(/apps/yuan/include)

# 指定要链接的静态库和动态库的位置
link_directories(/apps/yuan/lib)

# 定义变量
# 多个文件的最好按首字母排好，养成良好习惯
set(LIB_SRC
    yuan/address.cc
    yuan/bytearray.cc
    yuan/config.cc
    yuan/fd_manager.cc
    yuan/fiber.cc
    yuan/hook.cc
    yuan/iomanager.cc
    yuan/log.cc
    yuan/scheduler.cc
    yuan/socket.cc
    yuan/thread.cc
    yuan/timer.cc
    yuan/util.cc
)

# 生成so文件，名为yuan的动态库
add_library(yuan SHARED ${LIB_SRC})
# 编译yuan动态库的时候会修改__FILE__的内容为相对路径
force_redefine_file_macro_for_sources(yuan)

set(LIB_LIB
    yuan
    dl
    pthread
    yaml-cpp)

# 生成测试文件
add_executable(test tests/test.cc)
# https://blog.csdn.net/KingOfMyHeart/article/details/112983922
add_dependencies(test yuan)
force_redefine_file_macro_for_sources(test)
target_link_libraries(test ${LIB_LIB})

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config yuan)
force_redefine_file_macro_for_sources(test_config)
target_link_libraries(test_config ${LIB_LIB})

add_executable(test_thread tests/test_thread.cc)
add_dependencies(test_thread yuan)
force_redefine_file_macro_for_sources(test_thread)
target_link_libraries(test_thread ${LIB_LIB})

add_executable(test_util tests/test_util.cc)
add_dependencies(test_util yuan)
force_redefine_file_macro_for_sources(test_util)
target_link_libraries(test_util ${LIB_LIB})

add_executable(test_fiber tests/test_fiber.cc)
add_dependencies(test_fiber yuan)
force_redefine_file_macro_for_sources(test_fiber)
target_link_libraries(test_fiber ${LIB_LIB})

add_executable(test_scheduler tests/test_scheduler.cc)
add_dependencies(test_scheduler yuan)
force_redefine_file_macro_for_sources(test_scheduler)
target_link_libraries(test_scheduler ${LIB_LIB})

add_executable(test_iomanager tests/test_iomanager.cc)
add_dependencies(test_iomanager yuan)
force_redefine_file_macro_for_sources(test_iomanager)
target_link_libraries(test_iomanager ${LIB_LIB})

add_executable(test_hook tests/test_hook.cc)
add_dependencies(test_hook yuan)
force_redefine_file_macro_for_sources(test_hook)
target_link_libraries(test_hook ${LIB_LIB})

add_executable(test_address tests/test_address.cc)
add_dependencies(test_address yuan)
force_redefine_file_macro_for_sources(test_address)
target_link_libraries(test_address ${LIB_LIB})

add_executable(test_socket tests/test_socket.cc)
add_dependencies(test_socket yuan)
force_redefine_file_macro_for_sources(test_socket)
target_link_libraries(test_socket ${LIB_LIB})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
