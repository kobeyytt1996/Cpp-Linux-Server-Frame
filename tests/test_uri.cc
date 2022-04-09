#include "../yuan/log.h"
#include "../yuan/uri.h"

#include <iostream>

int main(int argc, char **argv) {
    yuan::Uri::ptr uri = yuan::Uri::CreateUri("http://www.sylar.top/test/uri?name=yuan&id=5#frag");
    std::cout << *uri << std::endl;

    auto addr = uri->createAddress();
    std::cout << *addr << std::endl;

    return 0;
}