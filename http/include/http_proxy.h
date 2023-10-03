#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

class HttpReverseProxy
{
public:
    HttpReverseProxy(const char *TargetHost, int TargetPort, int ProxyPort);

    ~HttpReverseProxy();

    // 启动反向代理
    void Start();

private:
    const char* TargetHost_;
    int TargetPort_;
    int ProxyPort_;
    int ServerSocket_;
    int epoll_fd;

    // 初始化
    void Init();

    // 处理客户端发出的请求
    void HandleClientRequest(int ClientSocket);

    // 清理代理
    void CleanUp();   
};

#endif