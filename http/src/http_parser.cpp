#include "http_parser.h"
#include <iostream>
#include <regex>
#include <string>
#include <cstring>
#include <netdb.h>
#include <arpa/inet.h>

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

    std::regex MethodRegex("Request Method: (\\w+)\\s*\\r?");
    std::regex URLRegex("Request URL: (https?://\\S+)\\s*\\r?"); // 修改正则表达式以匹配 "https" 或 "http"

    std::smatch MethodMatch;
    std::smatch URLMatch;

    if (std::regex_search(RequestLine, MethodMatch, MethodRegex))
    {
        request.method = MethodMatch[1];
    }

    if (std::regex_search(RequestLine, URLMatch, URLRegex))
    {
        request.url = URLMatch[1];
    }
}

void HttpRequestParser::ParseHeaders(HttpRequest &request)
{
    size_t HeadersStart = HttpRequestText_.find("\r\n\r\n") + 4; // 跳过请求行后的换行符
    size_t pos = HttpRequestText_.find("\r\n", HeadersStart);
    std::string HeadersText = HttpRequestText_.substr(pos + 2);

    size_t HeaderStart = 0;
    while (HeaderStart < HeadersText.length())
    {
        size_t HeaderEnd = HeadersText.find("\r\n", HeaderStart);
        if (HeaderEnd == std::string::npos)
            break;
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

void HttpRequestParser::ReplaceIPPort(std::string &url, const std::string &newHost)
{
    // 使用正则表达式查找 "http(s)://" 后面的部分并替换为新主机名
    std::regex regex("http[s]?://(?:[^/]+|[^:]+)(?=/|$)");
    url = std::regex_replace(url, regex, newHost);
}

std::string HttpRequestParser::ParseURL(std::string url)
{
    // 查找第一个斜杠的位置
    size_t slashPos = url.find("/", 8);
    std::string URL;

    if (slashPos != std::string::npos)
    {
        // 获取从斜杠开始的部分
        URL = url.substr(slashPos);
    }
    else
    {
        URL = "";
    }

    return URL;
}

void HttpRequestParser::ReplaceURL(std::string &URL, const std::string &NewURL)
{
    size_t StartPos = URL.find("://") + 3;
    size_t pos = URL.find('/', StartPos);

    if (pos != std::string::npos)
        URL = URL.substr(0, pos);

    URL += NewURL;
}

// 解析 IP
std::string HttpRequestParser::ParseHost(std::string url)
{
    std::string domain;
    // 从URL中提取域名
    size_t start = url.find("://");
    if (start != std::string::npos)
    {
        start += 3; // 跳过 "://"
        size_t end = url.find("/", start);
        if (end != std::string::npos)
        {
            domain = url.substr(start, end - start);
        }
        else
        {
            domain = url.substr(start);
        }

        size_t pos = domain.find(':');

        if (pos != std::string::npos)
            domain = domain.substr(0, pos);
    }

    return domain;
}

// 解析 Port
std::string HttpRequestParser::ParsePort(std::string url)
{
    // 检查 URL 是否以 "https://" 开头
    if (url.find("https://") == 0)
    {
        std::string IPAddress;
        std::string domain;
        // 从URL中提取域名
        size_t start = url.find("://");
        if (start != std::string::npos)
        {
            start += 3; // 跳过 "://"
            size_t end = url.find("/", start);
            if (end != std::string::npos)
            {
                domain = url.substr(start, end - start);
            }
            else
            {
                domain = url.substr(start);
            }

            size_t pos = domain.find(":");

            if (pos != std::string::npos)
                return domain.substr(pos + 1);
            else
                return "443"; // 默认 Https 端口
        }
    }
    else if (url.find("http://") == 0)
    {
        std::string IPAddress;
        std::string domain;
        // 从URL中提取域名
        size_t start = url.find("://");
        if (start != std::string::npos)
        {
            start += 3; // 跳过 "://"
            size_t end = url.find("/", start);
            if (end != std::string::npos)
            {
                domain = url.substr(start, end - start);
            }
            else
            {
                domain = url.substr(start);
            }

            size_t pos = domain.find(":");

            if (pos != std::string::npos)
                return domain.substr(pos + 1);
        }
    }

    return "80"; // 默认 Http 端口
}

// 判断是否是 ipv4 或者ipv6
bool IsIPAddress(const std::string &ServerName)
{
    // 使用正则表达式匹配IPv4地址的模式
    std::regex ipv4Pattern("^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$");

    // 使用正则表达式匹配IPv6地址的模式
    std::regex ipv6Pattern("^([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}$|^([0-9a-fA-F]{1,4}:){1,7}:|^([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}$|^(\\?i)ipv6:(://)?([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}$");

    // 如果匹配成功，就认为是IP地址
    return std::regex_match(ServerName, ipv4Pattern) || std::regex_match(ServerName, ipv6Pattern);
}

// 函数用于将域名解析为IP地址
std::string ResolveDomainToIP(const std::string &domain)
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // 使用IPv4或IPv6
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(domain.c_str(), NULL, &hints, &res);
    if (status != 0)
    {
        std::cerr << "DNS resolution error: " << gai_strerror(status) << std::endl;
        return ""; // 返回空字符串表示解析失败
    }

    char ipStr[INET6_ADDRSTRLEN];
    void *addr;
    if (res->ai_family == AF_INET)
    {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        addr = &(ipv4->sin_addr);
    }
    else
    {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
        addr = &(ipv6->sin6_addr);
    }

    // 将二进制地址转换为文本形式
    inet_ntop(res->ai_family, addr, ipStr, sizeof(ipStr));

    freeaddrinfo(res);

    return ipStr;
}

int GetHttpResponseStatusCode(const char *response)
{
    // 将 char* response 转换为 std::string
    std::string responseStr(response);

    // 查找第一个空格的位置
    size_t firstSpacePos = responseStr.find(' ');

    // 如果找到空格并且后面还有字符
    if (firstSpacePos != std::string::npos && firstSpacePos < responseStr.length() - 1)
    {
        // 提取状态码部分，状态码通常是三个数字
        std::string statusCode = responseStr.substr(firstSpacePos + 1, 3);

        // 将状态码字符串转换为整数并返回
        try
        {
            int statusCodeInt = std::stoi(statusCode);
            return statusCodeInt;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error converting status code to integer: " << e.what() << std::endl;
        }
    }

    // 如果无法提取状态码，返回默认值（例如，-1 表示失败）
    return -1;
}