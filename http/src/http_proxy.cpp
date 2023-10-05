#include "http_proxy.h"
#include "http_parser.h"
#include "conf_parser.h"
#include "log.h"
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <regex>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <random>
#include <functional>

extern std::map<std::string, Logger> LogMap;

HttpProxy::HttpProxy(std::map<std::string, std::vector<ServerBlock>> ServerMap, std::vector<upstream> Upstreams) : ServerMap_(ServerMap), Upstreams_(Upstreams), epoll_fd(-1)
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

    for (auto it : ServerMap_)
    {
        // 创建 server socket
        int ServerSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (ServerSocket_ < 0)
        {
            std::cerr << "Socket creation error" << std::endl;
            exit(EXIT_FAILURE);
        }

        ServerSockets_.push_back(ServerSocket_);

        // 绑定服务器 Socket 到指定端口
        struct sockaddr_in ServerAddr;
        memset(&ServerAddr, 0, sizeof(ServerAddr));
        ServerAddr.sin_family = AF_INET;
        ServerAddr.sin_addr.s_addr = INADDR_ANY;
        ServerAddr.sin_port = htons(std::stoi(it.first));

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

        std::cout << "Reverse proxy is listening on port " << it.first << "..." << std::endl;
    }
}

void HttpProxy::CleanUp()
{
    for (auto ServerSocket_ : ServerSockets_)
    {
        if (ServerSocket_ >= 0)
            close(ServerSocket_);
    }

    if (epoll_fd > 0)
        close(epoll_fd);
}

void HttpProxy::Start()
{
    epoll_event *EpollEvents = (epoll_event *)malloc(sizeof(struct epoll_event) * 100);

    while (true)
    {
        // 等待事件
        int EventsNumber = epoll_wait(epoll_fd, EpollEvents, 100, -1);
        if (EventsNumber == -1)
        {
            std::cerr << "Epoll wait error" << std::endl;
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < EventsNumber; ++i)
        {
            int fd = EpollEvents[i].data.fd;

            if (std::find(ServerSockets_.begin(), ServerSockets_.end(), fd) != ServerSockets_.end())
            {
                // 有新的客户端请求连接
                struct sockaddr_in ClientAddr;
                socklen_t ClientLen = sizeof(ClientAddr);
                int ClientSocket = accept(fd, (struct sockaddr *)&ClientAddr, &ClientLen);
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
    char buffer[4096] = {'\0'};
    int BytesReceived = recv(ClientSocket, buffer, sizeof(buffer), 0);
    if (BytesReceived <= 0)
    {
        std::cerr << "Received nothing from client" << std::endl;
        return;
    }

    MatchServer(ClientSocket, buffer);
}

void HttpProxy::MatchServer(int ClientSocket, const char *buffer)
{
    // 解析 http 报文
    std::string ClientRequest(buffer);

    HttpRequestParser parser(ClientRequest);
    HttpRequest request_temp_ = parser.Parse();
    std::string HostName = parser.ParseHost(request_temp_.url);
    std::string Port = parser.ParsePort(request_temp_.url);

    if (ServerMap_.find(Port) != ServerMap_.end())
    {
        std::vector<ServerBlock> Servers = ServerMap_[Port];

        // 精确匹配
        for (const auto &Server : Servers)
        {
            for (const auto ServerName : Server.server_names)
            {
                if (MatchServerNameExact(ServerName, HostName))
                {
                    MatchLocation(ClientSocket, request_temp_, Server, parser);
                    return;
                }
            }
        }

        // 通配符匹配
        for (const auto &Server : Servers)
        {
            for (const auto ServerName : Server.server_names)
            {
                if (MatchServerNameWildcard(ServerName, HostName))
                {
                    MatchLocation(ClientSocket, request_temp_, Server, parser);
                    return;
                }
            }
        }

        // 正则表达式匹配
        for (const auto &Server : Servers)
        {
            for (const auto ServerName : Server.server_names)
            {
                if (MatchServerNameRegex(ServerName, HostName))
                {
                    MatchLocation(ClientSocket, request_temp_, Server, parser);
                    return;
                }
            }
        }
    }
    else
    {
        // 错误处理
        std::cerr << "Invalid access port" << std::endl;
    }
}

void HttpProxy::MatchLocation(int ClientSocket, HttpRequest Request, ServerBlock Server, HttpRequestParser parser)
{
    std::string URL = parser.ParseURL(Request.url);
    HttpRequest requestFromClient = Request;

    LocationBlock SelectedLocation;

    // 优先级从高到低排序
    std::vector<LocationBlock> sortedLocations = Server.locations;

    std::sort(sortedLocations.begin(), sortedLocations.end(), [](const LocationBlock &a, const LocationBlock &b)
              {
        // 根据优先级排序，例如，"=" 最高，"^~" 次之，然后是 "~" 和 "~*"
        // 这里可以根据你的需求调整优先级顺序
        std::map<std::string, int> priorityMap = {{"= ", 0}, {"^~", 1}, {"~ ", 2}, {"~*", 2}, {"/", 3}};

        std::string RuleA = a.path.substr(0, 2);
        std::string RuleB = b.path.substr(0, 2);

        if (RuleA.find('/') != std::string::npos) RuleA = "/";
        if (RuleB.find('/') != std::string::npos) RuleB = "/";

        int priorityA = priorityMap[RuleA];
        int priorityB = priorityMap[RuleB];

        if (priorityA == priorityB && priorityA != 2)
        {
            // 如果优先级相同，按路径长度从多到少排序
            return a.path.length() > b.path.length();
        }
        else
        {
            // 否则按优先级排序(注意把正则匹配最后的一个匹配往前放)
            return priorityA <= priorityB;
        } });

    for (const auto &location : sortedLocations)
    {
        size_t pos = location.path.find(" ");
        if (pos != std::string::npos)
        {
            std::string flag = location.path.substr(0, pos);
            std::string path = location.path.substr(pos + 1);

            if (flag == "=")
            {
                // 精确匹配
                if (URL == path)
                {
                    URL = "";
                    SelectedLocation = location;
                    break; // 匹配成功，退出循环
                }
            }
            else if (flag == "^~")
            {
                // 前缀匹配
                if (URL.find(path) == 0)
                {
                    // 去掉已匹配的部分
                    URL = URL.substr(path.length());
                    SelectedLocation = location;
                    break; // 匹配成功，退出循环
                }
            }
            else if (flag == "~")
            {
                // 区分大小写的正则匹配
                std::regex regex(path);
                if (std::regex_search(URL, regex))
                {
                    // 使用正则表达式替换已匹配的部分为空字符串
                    URL = std::regex_replace(URL, regex, "");
                    SelectedLocation = location;
                    break; // 匹配成功，退出循环
                }
            }
            else if (flag == "~*")
            {
                // 不区分大小写的正则匹配
                std::regex regex(path, std::regex_constants::icase);
                if (std::regex_search(URL, regex))
                {
                    // 使用正则表达式替换已匹配的部分为空字符串
                    URL = std::regex_replace(URL, regex, "");
                    SelectedLocation = location;
                    break; // 匹配成功，退出循环
                }
            }
        }
        else
        {
            // 通用匹配
            if (URL.find(location.path) == 0)
            {
                // 去掉已匹配的部分
                URL = URL.substr(location.path.length());
                SelectedLocation = location;
                break; // 匹配成功，退出循环
            }

            if (location.path == "/")
            {
                SelectedLocation = location;
                break; // 匹配成功，退出循环
            }
        }
    }

    if (SelectedLocation.path.empty())
    {
        LogMap[Server.error_log].LogError(ERROR, "Failed to select location");
        return;
    }

    parser.ReplaceURL(Request.url, URL);

    HandleRequest(ClientSocket, Request, requestFromClient, SelectedLocation, parser, Server);
}

void HttpProxy::HandleRequest(int ClientSocket, HttpRequest Request, HttpRequest requestFromClient, LocationBlock location, HttpRequestParser parser, ServerBlock Server)
{
    if (!location.proxy_pass.empty())
    {
        for (const auto Upstream : Upstreams_)
        {
            if (location.proxy_pass.find(Upstream.host) != std::string::npos)
                location.proxy_pass = LoadBalancingGetProxyPass(Upstream, ClientSocket);
        }

        // 反向代理请求到指定的目标服务器
        std::string TargetIP;
        int TargetPort;
        std::map<std::string, std::string> Headers;
        ExtractIpPort(location.proxy_pass, TargetIP, TargetPort);
        ExtractHeader(Headers, location.proxy_set_header);
        ReverseProxy ReverseProxy_(TargetIP.c_str(), TargetPort, Headers);
        ReverseProxy_.ReverseProxyRequest(ClientSocket, parser.ToHttpRequestText(Request).c_str(), Server, requestFromClient);
    }
    else if (!location.root.empty())
    {
        // 代理静态资源
        std::string FileName = location.root;
        std::string URL = parser.ParseURL(Request.url);

        if (URL.empty())
        {
            FileName = FileName + "/" + location.index;
        }
        else
        {
            FileName += URL;
        }

        StaticResourcesProxy StaticResourcesProxy_(FileName);
        StaticResourcesProxy_.StaticResourcesProxyRequest(ClientSocket, Server, requestFromClient);
        return;
    }
}

// 负载均衡算法，选择相应的代理 ip 和 port
std::string HttpProxy::LoadBalancingGetProxyPass(upstream Upstream, int ClientSocket)
{
    std::string SelectedProxyPass;

    switch (Upstream.method)
    {
    case RoundRobin:
        LoadBalancingRoundRobin(Upstream.servers, SelectedProxyPass);
        break;
    case WeightedRoundRobin:
        LoadBalancingWeightRoundRobin(Upstream.servers, SelectedProxyPass);
        break;
    case IPHash:
        LoadBalancingIPHash(Upstream.servers, SelectedProxyPass, ClientSocket);
        break;
    }

    return SelectedProxyPass;
}

// 轮询
void HttpProxy::LoadBalancingRoundRobin(std::vector<std::string> Servers, std::string &ProxyPass)
{
    static int index = 0;

    ProxyPass = Servers[index];

    index = (index + 1) % Servers.size();
}

// 加权轮询
void HttpProxy::LoadBalancingWeightRoundRobin(std::vector<std::string> Servers, std::string &ProxyPass)
{
    std::vector<std::pair<std::string, int>> WeightedServers;
    int totalWeight = 0; // 计算总权重

    for (const auto &server : Servers)
    {
        size_t weightPos = server.find(" weight=");
        if (weightPos != std::string::npos)
        {
            int weight = std::stoi(server.substr(weightPos + 8));
            totalWeight += weight;
            WeightedServers.emplace_back(server.substr(0, weightPos), weight);
        }
        else
        {
            totalWeight += 1;
            WeightedServers.emplace_back(server, 1);
        }
    }

    // 生成随机数来选择服务器
    static std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(1, totalWeight);
    int randomNumber = distribution(generator);

    // 根据随机数和权重选择服务器
    int CurrentWeight = 0;
    for (const auto &Server : WeightedServers)
    {
        CurrentWeight += Server.second;
        if (randomNumber <= CurrentWeight)
        {
            ProxyPass = Server.first;
            break;
        }
    }
}

// IP哈希
void HttpProxy::LoadBalancingIPHash(std::vector<std::string> Servers, std::string &ProxyPass, int ClientSocket)
{
    // 假设 clientSocket 是已连接的套接字
    struct sockaddr_in clientAddr;
    socklen_t AddrLen = sizeof(clientAddr);

    if (getpeername(ClientSocket, (struct sockaddr *)&clientAddr, &AddrLen) == 0)
    {
        char ClientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), ClientIP, INET_ADDRSTRLEN);
        
        // 使用哈希函数计算客户端IP地址的哈希值
        std::hash<char *> HashFunction;
        size_t HashValue = HashFunction(ClientIP);

        // 使用哈希值来选择服务器
        size_t ServerIndex = HashValue % Servers.size();
        ProxyPass = Servers[ServerIndex];
    }
}

// 精确匹配
bool HttpProxy::MatchServerNameExact(const std::string &pattern, const std::string &HostName)
{
    return pattern == HostName;
}

// 通配符匹配
bool HttpProxy::MatchServerNameWildcard(const std::string &pattern, const std::string &HostName)
{
    std::regex wildcard(pattern);
    return std::regex_match(HostName, wildcard);
}

// 正则表达式匹配
bool HttpProxy::MatchServerNameRegex(const std::string &pattern, const std::string &HostName)
{
    try
    {
        std::regex regex(pattern);
        return std::regex_match(HostName, regex);
    }
    catch (std::regex_error &)
    {
        // 忽略无效的正则表达式
        return false;
    }
}

ReverseProxy::ReverseProxy(const char *TargetHost, int TargetPort, std::map<std::string, std::string> Headers)
    : TargetHost_(TargetHost), TargetPort_(TargetPort), Headers_(Headers) {}

void ReverseProxy::ReverseProxyRequest(int ClientSocket, const char *buffer, ServerBlock Server, HttpRequest requestFromClient)
{
    // 创建 socket 连接到目标服务器
    int TargetSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (TargetSocket < 0)
    {
        LogMap[Server.error_log].LogError(ERROR, "Target socket creation error in reverse proxy");
        return;
    }

    struct sockaddr_in TargetAddr;
    memset(&TargetAddr, 0, sizeof(TargetAddr));
    TargetAddr.sin_family = AF_INET;
    TargetAddr.sin_port = htons(TargetPort_);
    inet_pton(AF_INET, TargetHost_, &(TargetAddr.sin_addr));

    if (connect(TargetSocket, (struct sockaddr *)&TargetAddr, sizeof(TargetAddr)) < 0)
    {
        LogMap[Server.error_log].LogError(ERROR, "Conneet to target proxy server failed");
        close(TargetSocket);
        return;
    }

    // 解析 http 报文
    std::string ClientRequest(buffer);

    // std::cout << "从客户端接收到的报文：" << std::endl;
    // std::cout << ClientRequest << std::endl;

    // 替换请求行中的 URL
    HttpRequestParser parser(ClientRequest);
    HttpRequest request_temp_ = parser.Parse();
    std::string Host = "http://" + std::string(TargetHost_) + ":" + std::to_string(TargetPort_);
    parser.ReplaceIPPort(request_temp_.url, Host);

    // 替换请求头
    for (const auto it : Headers_)
    {
        request_temp_.headers[it.first] = it.second;
    }

    std::string request = parser.ToHttpRequestText(request_temp_);

    // std::cout << "解析转发的报文：" << std::endl;
    // std::cout << request;

    // 转发到目标服务器
    send(TargetSocket, request.c_str(), strlen(request.c_str()), 0);

    // 接收目标服务器的响应并且转发给客户端
    char response[4096];
    int ResponseReceived = recv(TargetSocket, response, sizeof(response), 0);

    int StatusCode = GetHttpResponseStatusCode(response);

    if (StatusCode == 200)
    {
        LogMap[Server.access_log].LogAcess(INFO, requestFromClient, response);
    }
    else
    {
        std::string ErrorMessage = "HTTP/1.1 " + std::to_string(StatusCode) + " ";
        switch (StatusCode)
        {
        case 400:
            ErrorMessage = "Bad Request";
            break;
        case 401:
            ErrorMessage = "Unauthorized";
            break;
        case 403:
            ErrorMessage = "Forbidden";
            break;
        case 404:
            ErrorMessage = "Not Found";
            break;
        case 500:
            ErrorMessage = "Internal Server Error";
            break;
        // 可以根据需要添加其他状态码的描述
        default:
            ErrorMessage = "Unknown Error";
            break;
        }

        LogMap[Server.error_log].LogError(ERROR, ErrorMessage);
    }

    if (ResponseReceived <= 0)
    {
        LogMap[Server.error_log].LogError(ERROR, "Received No Response from proxy server");
        close(TargetSocket);
        return;
    }

    send(ClientSocket, response, sizeof(response), 0);

    close(TargetSocket);
}

StaticResourcesProxy::StaticResourcesProxy(const std::string FilePath) : FilePath_(FilePath) {}

void StaticResourcesProxy::StaticResourcesProxyRequest(int ClientSocket, ServerBlock Server, HttpRequest requestFromClient)
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

        char FileBuffer[4096];

        while (!File.eof())
        {
            File.read(FileBuffer, sizeof(FileBuffer));
            send(ClientSocket, FileBuffer, File.gcount(), 0);
        }

        LogMap[Server.access_log].LogAcess(INFO, requestFromClient, response.c_str());
    }
    else
    {
        // 文件不存在，返回404错误
        std::string notFoundResponse = "HTTP/1.1 404 Not Found\r\n\r\n";
        LogMap[Server.error_log].LogError(ERROR, notFoundResponse);
        send(ClientSocket, notFoundResponse.c_str(), notFoundResponse.size(), 0);
    }
}

// 从 ip:port中提取出 ip 和 port
void ExtractIpPort(const std::string &ipPortStr, std::string &ip, int &port)
{
    // 使用 ':' 分割字符串
    size_t colonPos = ipPortStr.find(':');
    if (colonPos != std::string::npos)
    {
        // 提取 IP 部分
        ip = ipPortStr.substr(0, colonPos);

        if (!IsIPAddress(ip))
            ip = ResolveDomainToIP(ip);

        // 提取 Port 部分并将其转换为整数
        std::string portStr = ipPortStr.substr(colonPos + 1);
        std::istringstream portStream(portStr);
        portStream >> port;
    }
    else
    {
        // 如果未找到 ':'，可以添加适当的错误处理
        std::cerr << "Invalid IP:Port format in proxy_pass, set port = 80" << std::endl;
        ip = ipPortStr;
        if (!IsIPAddress(ip))
            ip = ResolveDomainToIP(ip);
        port = 80;
    }
}

// 从 proxy_set_header 中提取键值对
void ExtractHeader(std::map<std::string, std::string> &Headers, std::string proxy_set_header)
{
    std::istringstream iss(proxy_set_header);
    std::string header;

    while (std::getline(iss, header, ','))
    {
        // 在这里处理每个键值对
        size_t colonPos = header.find(" ");
        if (colonPos != std::string::npos)
        {
            std::string key = header.substr(0, colonPos);
            std::string value = header.substr(colonPos + 1);

            Headers[key] = value;
        }
    }
}