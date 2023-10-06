#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

#include "conf_parser.h"
#include "http_parser.h"
#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <regex>

class HttpProxy
{
public:
    HttpProxy(int NumWorkers, std::map<std::string, std::vector<ServerBlock>> ServerMap, std::vector<upstream> Upstreams);

    ~HttpProxy();

    // 启动代理
    void Start();

private:
    std::map<std::string, std::vector<ServerBlock>> ServerMap_;
    std::vector<int> ServerSockets_;
    std::vector<upstream> Upstreams_;
    std::vector<pid_t> WorkerProcesses;
    std::vector<int> worker_sock_fd_;
    int epoll_fd_master;
    int NumWorkers_;

    // 初始化
    void Init();

    // 清除监听
    void CleanUp();

    // Worker进程的函数
    void WorkerFunction(int workerId, int worker_sock_fd);

    // 处理客户端发出的请求
    void ProxyRequest(int ClientSocket);

    // Server 规则
    void MatchServer(int ClientSocket, const char *buffer);

    // Location 规则
    void MatchLocation(int ClientSocket, HttpRequest Request, ServerBlock Server, HttpRequestParser parser);

    // 处理请求是反向代理还是静态资源代理
    void HandleRequest(int ClientSocket, HttpRequest Request, HttpRequest requestFromClient, LocationBlock location, HttpRequestParser parser, ServerBlock Server);

    // 负载均衡算法，选择相应的代理 ip 和 port
    std::string LoadBalancingGetProxyPass(upstream Upstream, int ClientSocket);

    // 轮询
    void LoadBalancingRoundRobin(std::vector<std::string> Servers, std::string &ProxyPass);

    // 加权轮询
    void LoadBalancingWeightRoundRobin(std::vector<std::string> Servers, std::string &ProxyPass);

    // IP哈希
    void LoadBalancingIPHash(std::vector<std::string> Servers, std::string &ProxyPass, int ClientSocket);

    // 精确匹配
    bool MatchServerNameExact(const std::string &pattern, const std::string &HostName);

    // 通配符匹配
    bool MatchServerNameWildcard(const std::string &pattern, const std::string &HostName);

    // 正则表达式匹配
    bool MatchServerNameRegex(const std::string &pattern, const std::string &HostName);
};

class ReverseProxy
{
public:
    ReverseProxy(const char *TargetHost, int TargetPort, std::map<std::string, std::string> Headers);

    // 处理客户端反向代理请求
    void ReverseProxyRequest(int ClientSocket, const char *buffer, ServerBlock Server, HttpRequest requestFromClient);

private:
    std::map<std::string, std::string> Headers_;
    const char *TargetHost_;
    int TargetPort_;
};

class StaticResourcesProxy
{
public:
    StaticResourcesProxy(const std::string FilePath);

    // 处理客户端文件请求信息
    void StaticResourcesProxyRequest(int ClientSocket, ServerBlock Server, HttpRequest requestFromClient);

private:
    const std::string FilePath_;
};

// 从 ip:port中提取出 ip 和 port
void ExtractIpPort(const std::string &ipPortStr, std::string &ip, int &port);

// 从 proxy_set_header 中提取键值对
void ExtractHeader(std::map<std::string, std::string> &Headers, std::string proxy_set_header);

#endif