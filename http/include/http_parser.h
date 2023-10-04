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

    // 解析 Http 报文
    HttpRequest Parse();

    // 将请求转换成报文
    std::string ToHttpRequestText(HttpRequest &request);

    // 替换 ip:port
    void ReplaceIPPort(std::string &input, const std::string &newHost);

    // 替换 URL
    void ReplaceURL(std::string &URL, const std::string &NewURL);


    // 解析 URL
    std::string ParseURL(std::string url);

    // 解析 IP
    std::string ParseHost(std::string url);

    // 解析 Port
    std::string ParsePort(std::string url);

private:
    std::string HttpRequestText_;

    void ParseRequestLine(HttpRequest &request);

    void ParseHeaders(HttpRequest &request);
};

// 判断是否是 ipv4 或者ipv6
bool IsIPAddress(const std::string &serverName);

// 函数用于将域名解析为IP地址
std::string ResolveDomainToIP(const std::string &domain);

#endif