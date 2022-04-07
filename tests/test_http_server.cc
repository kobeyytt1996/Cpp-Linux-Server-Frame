#include "../yuan/http/http_server.h"
#include "../yuan/log.h"

void run() {
    yuan::Address::ptr addr = yuan::Address::LookupAny("0.0.0.0:8020");
    yuan::http::HttpServer::ptr server(new yuan::http::HttpServer(false));
    while (!server->bind(addr)) {
        sleep(2);
    }
    server->start();
}

int main(int argc, char **argv) {
    yuan::IOManager iom(2);
    iom.schedule(run);

    return 0;
}