#include "http_proxy.h"
#include "http_parser.h"
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

HttpProxy::HttpProxy(int ProxyPort) : ProxyPort_(ProxyPort), ServerSocket_(-1), epoll_fd(-1)
{
    Init();
}
HttpProxy::~HttpProxy()
{
    CleanUp();
}

void HttpProxy::Init()
{
    epoll_fd = epoll_create(1);
    if (epoll_fd == -1)
    {
        std::cerr << "Epoll create error" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 创建 server socket
    ServerSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (ServerSocket_ < 0)
    {
        std::cerr << "Socket creation error" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 绑定服务器 Socket 到指定端口
    struct sockaddr_in ServerAddr;
    memset(&ServerAddr, 0, sizeof(ServerAddr));
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(ProxyPort_);

    if (bind(ServerSocket_, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) < 0)
    {
        std::cerr << "Bind error" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 监听连接
    if (listen(ServerSocket_, 10) < 0)
    {
        std::cerr << "Listen error" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 将 sever socket 加入 epoll 监听事件
    struct epoll_event ServerEvent;
    ServerEvent.events = EPOLLIN; // 监听可读事件
    ServerEvent.data.fd = ServerSocket_;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ServerSocket_, &ServerEvent) == -1)
    {
        std::cerr << "Epoll control error in adding server socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Reverse proxy is listening on port " << ProxyPort_ << "..." << std::endl;
}

void HttpProxy::CleanUp()
{
    if (ServerSocket_ >= 0)
        close(ServerSocket_);

    if (epoll_fd > 0)
        close(epoll_fd);
}

void HttpProxy::Start()
{
    epoll_event *EpollEvents = (epoll_event *)malloc(sizeof(struct epoll_event) * 50);

    while (true)
    {
        // 等待事件
        int EventsNumber = epoll_wait(epoll_fd, EpollEvents, 50, -1);
        if (EventsNumber == -1)
        {
            std::cerr << "Epoll wait error" << std::endl;
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < EventsNumber; ++i)
        {
            int fd = EpollEvents[i].data.fd;

            if (fd == ServerSocket_)
            {
                // 有新的客户端请求连接
                struct sockaddr_in ClientAddr;
                socklen_t ClientLen = sizeof(ClientAddr);
                int ClientSocket = accept(ServerSocket_, (struct sockaddr *)&ClientAddr, &ClientLen);
                if (ClientSocket < 0)
                {
                    std::cerr << "Accept from client error" << std::endl;
                    continue;
                }

                // 将新的 client socket 加入到 epoll 监听
                struct epoll_event ClientEvent;
                ClientEvent.events = EPOLLIN;
                ClientEvent.data.fd = ClientSocket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ClientSocket, &ClientEvent) == -1)
                {
                    std::cerr << "Epoll control error in adding client" << std::endl;
                    close(ClientSocket);
                }
            }
            else
            {
                // 处理客户端请求,完成后关闭客户端
                ProxyRequest(fd);

                // 从 epoll 监听中删除客户端事件
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
                {
                    std::cerr << "Epoll control error in deleting client" << std::endl;
                }

                close(fd);
            }
        }
    }

    free(EpollEvents);
}

void HttpProxy::ProxyRequest(int ClientSocket)
{
    // 接受来自客户端的消息
    char buffer[1024];
    int BytesReceived = recv(ClientSocket, buffer, sizeof(buffer), 0);
    if (BytesReceived <= 0)
    {
        std::cerr << "Received nothing from client" << std::endl;
        return;
    }

    // ReverseProxy ReverseProxy_("127.0.0.1", 7000);
    // ReverseProxy_.ReverseProxyRequest(ClientSocket, buffer, BytesReceived);

    StaticResourcesProxy StaticResourcesProxy_("/home/cxj/test.html");
    StaticResourcesProxy_.StaticResourcesProxyRequest(ClientSocket);
}

ReverseProxy::ReverseProxy(const char *TargetHost, int TargetPort)
    : TargetHost_(TargetHost), TargetPort_(TargetPort) {}

void ReverseProxy::ReverseProxyRequest(int ClientSocket, const char *buffer, const int BytesReceived)
{
    // 创建 socket 连接到目标服务器
    int TargetSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (TargetSocket < 0)
    {
        std::cerr << "Target socket creation error in reverse proxy" << std::endl;
        return;
    }

    struct sockaddr_in TargetAddr;
    memset(&TargetAddr, 0, sizeof(TargetAddr));
    TargetAddr.sin_family = AF_INET;
    TargetAddr.sin_port = htons(TargetPort_);
    inet_pton(AF_INET, TargetHost_, &(TargetAddr.sin_addr));

    if (connect(TargetSocket, (struct sockaddr *)&TargetAddr, sizeof(TargetAddr)) < 0)
    {
        std::cerr << "Conneet to target proxy server failed" << std::endl;
        close(TargetSocket);
        return;
    }

    // 解析 http 报文
    std::string ClientRequest(buffer);

    std::cout << "从客户端接收到的报文：" << std::endl;
    std::cout << ClientRequest << std::endl;

    HttpRequestParser parser(ClientRequest);
    HttpRequest request_temp_ = parser.Parse();
    std::string Host = "http://" + std::string(TargetHost_) + ":" + std::to_string(TargetPort_);
    parser.ReplaceHost(request_temp_.url, Host);
    std::string request = parser.ToHttpRequestText(request_temp_);

    std::cout << "解析转发的报文：" << std::endl;
    std::cout << request;

    // 转发到目标服务器
    send(TargetSocket, request.c_str(), BytesReceived, 0);

    // 接收目标服务器的响应并且转发给客户端
    char response[1024];
    int ResponseReceived = recv(TargetSocket, response, sizeof(response), 0);

    if (ResponseReceived <= 0)
    {
        std::cerr << "Received No Response from proxy server" << std::endl;
        close(TargetSocket);
        return;
    }

    send(ClientSocket, response, sizeof(response), 0);

    close(TargetSocket);
}

StaticResourcesProxy::StaticResourcesProxy(const std::string FilePath) : FilePath_(FilePath) {}

void StaticResourcesProxy::StaticResourcesProxyRequest(int ClientSocket)
{
    std::ifstream File(FilePath_, std::ios::binary);
    if (File)
    {
        std::string response;
        response += "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";

        send(ClientSocket, response.c_str(), response.size(), 0);

        char FileBuffer[1024];

        while (!File.eof())
        {
            File.read(FileBuffer, sizeof(FileBuffer));
            send(ClientSocket, FileBuffer, File.gcount(), 0);
        }
    }
    else
    {
        // 文件不存在，返回404错误
        std::string notFoundResponse = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(ClientSocket, notFoundResponse.c_str(), notFoundResponse.size(), 0);
    }
}