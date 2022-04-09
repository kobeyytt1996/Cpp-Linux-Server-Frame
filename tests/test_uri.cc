#include "../yuan/log.h"
#include "../yuan/uri.h"

#include <iostream>

int main(int argc, char **argv) {
    // 注意中文需要在ragel里加特殊的支持。可以在uri.rl里搜chinese
    yuan::Uri::ptr uri = yuan::Uri::CreateUri("http://admin@www.sylar.top/test/uri?name=yuan中文&id=5#frag");
    std::cout << *uri << std::endl;

    auto addr = uri->createAddress();
    std::cout << *addr << std::endl;

    return 0;
}