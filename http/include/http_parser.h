#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <string>
#include <vector>
#include <map>

struct HttpRequest
{
    std::string method;
    std::string url;
    std::map<std::string, std::string> headers;
};

class HttpRequestParser
{
public:
    HttpRequestParser(const std::string &HttpRequestText);

    HttpRequest Parse();

private:
    std::string HttpRequestText_;

    void ParseRequestLine(HttpRequest &request);

    void ParseHeaders(HttpRequest &request);
};

#endif