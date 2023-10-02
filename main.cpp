#include "http_parser.h"
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
    std::string httpRequestText = "Request URL: http://localhost/api/v1/healthy\r\n"
                             "Request Method: GET\r\n\r\n"
                             "Request Header:\r\n"
                             "Host: localhost\r\n"
                             "User-Agent: Mozilla/5.0 (Windows NT 10.0; Wisn64; x64) AppleWebKit/537.36 (KHTML,like Gecko) Chrome/77.0.3865.90Safari/537.36\r\n"
                             "Accept: */*\r\n";

    HttpRequestParser httpRequestParser(httpRequestText);
    HttpRequest request = httpRequestParser.Parse();

    printHttpRequest(request);

    return 0;
}