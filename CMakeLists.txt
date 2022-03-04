# 调用make clean
cmake_minimum_required(VERSION 2.8)
project(yuan)

# make过程中显示详细命令
set(CMAKE_VERBOSE_MAKEFILE ON)
# 自定义的编译参数 http://blog.sina.com.cn/s/blog_553230d70101efqv.html
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unuesed-function")

include_directories(.)
include_directories(/usr/local/boost/include/)
include_directories(/apps/yuan/include)

# 指定要链接的静态库和动态库的位置
link_directories(/apps/yuan/lib)

# 定义变量
set(LIB_SRC
    yuan/log.cc   
    yuan/util.cc 
    yuan/config.cc
)

# 生成so文件，名为yuan的动态库
add_library(yuan SHARED ${LIB_SRC})

# 生成测试文件
add_executable(test tests/test.cc)
# https://blog.csdn.net/KingOfMyHeart/article/details/112983922
add_dependencies(test yuan)
target_link_libraries(test yuan)

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config yuan)
target_link_libraries(test_config yuan yaml-cpp)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
