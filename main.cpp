#include "http_parser.h"
#include "http_proxy.h"
#include <iostream>

void printHttpRequest(const HttpRequest &request) {
    std::cout << "Method: " << request.method << std::endl;
    std::cout << "URL: " << request.url << std::endl;
    std::cout << "Headers:" << std::endl;
    
    for (const auto& header : request.headers) {
        std::cout << "  " << header.first << ": " << header.second << std::endl;
    }
}

int main()
{
    HttpReverseProxy ServerReverseProxy("127.0.0.1", 7000, 80);
    ServerReverseProxy.Start();

    return 0;
}