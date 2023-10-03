#include "http_parser.h"
#include <iostream>
#include <regex>

HttpRequestParser::HttpRequestParser(const std::string &HttpRequestText) : HttpRequestText_(HttpRequestText) {}

HttpRequest HttpRequestParser::Parse()
{
    HttpRequest request;

    // 解析请求行
    ParseRequestLine(request);

    // 解析请求头
    ParseHeaders(request);

    return request;
}

void HttpRequestParser::ParseRequestLine(HttpRequest &request)
{
    size_t RequestLineEnd = HttpRequestText_.find("\r\n\r\n") + 2;
    std::string RequestLine = HttpRequestText_.substr(0, RequestLineEnd);

    size_t URLEnd = RequestLine.find("\r\n");

    if (URLEnd != std::string::npos)
    {
        size_t Separator = RequestLine.find(':');

        request.url = RequestLine.substr(Separator + 2, URLEnd - Separator - 2);

        size_t MethodStart = URLEnd + 1;
        size_t MethodEnd = RequestLine.find("\r\n", MethodStart);
        Separator = RequestLine.find(':', MethodStart);
        if (MethodEnd != std::string::npos)
        {
            request.method = RequestLine.substr(Separator + 2, MethodEnd - Separator - 2); // 跳过冒号和空格
        }
    }
}

void HttpRequestParser::ParseHeaders(HttpRequest &request)
{
    size_t HeadersStart = HttpRequestText_.find("\r\n\r\n") + 4; // 跳过请求行后的换行符
    std::string HeadersText = HttpRequestText_.substr(HeadersStart + 17);

    size_t HeaderStart = 0;
    while (HeaderStart < HeadersText.length())
    {
        size_t HeaderEnd = HeadersText.find("\r\n", HeaderStart);
        std::string HeaderLine = HeadersText.substr(HeaderStart, HeaderEnd - HeaderStart);

        size_t Separator = HeaderLine.find(':');
        if (Separator != std::string::npos)
        {
            std::string Name = HeaderLine.substr(0, Separator);
            std::string Value = HeaderLine.substr(Separator + 2); // 跳过冒号和空格
            request.headers[Name] = Value;
        }

        HeaderStart = HeaderEnd + 2; // 跳过换行符
    }
}

std::string HttpRequestParser::ToHttpRequestText(HttpRequest &request)
{
    std::string HttpRequestText =
        "Request URL: " + request.url + "\r\n" +
        "Request Method: " + request.method + "\r\n\r\n" +
        "Request Header:\r\n";

    for (const auto &header : request.headers)
    {
        HttpRequestText += header.first + ": " + header.second + "\r\n";
    }

    return HttpRequestText;
}

void HttpRequestParser::ReplaceHost(std::string &url, const std::string &newHost)
{
    // 使用正则表达式查找 "http://" 后面的部分并替换为新主机名
    std::regex regex("http[s]?://[^/]+");
    url = std::regex_replace(url, regex, newHost);
}