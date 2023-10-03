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

    // 解析Http报文
    HttpRequest Parse();

    // 将请求转换成报文
    std::string ToHttpRequestText(HttpRequest &request);

    // 替换Host
    void ReplaceHost(std::string &input, const std::string &newHost);

private:
    std::string HttpRequestText_;

    void ParseRequestLine(HttpRequest &request);

    void ParseHeaders(HttpRequest &request);
};

#endif