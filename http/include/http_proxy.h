#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

#include <string>

class HttpProxy
{
public:
    HttpProxy(int ProxyPort);

    ~HttpProxy();

    // 启动代理
    void Start();

private:
    int ProxyPort_;
    int ServerSocket_;
    int epoll_fd;

    // 初始化
    void Init();

    // 清除监听
    void CleanUp();

    // 处理客户端发出的请求
    void ProxyRequest(int ClientSocket);
};

class ReverseProxy
{
public:
    ReverseProxy(const char *TargetHost, int TargetPort);

    // 处理客户端反向代理请求
    void ReverseProxyRequest(int ClientSocket, const char *buffer, const int BytesReceived); 

private:
    const char* TargetHost_;
    int TargetPort_;
};

class StaticResourcesProxy
{
public:
    StaticResourcesProxy(const std::string FilePath);

    // 处理客户端文件请求信息
    void StaticResourcesProxyRequest(int ClientSocket);

private:
    const std::string FilePath_;
};

#endif