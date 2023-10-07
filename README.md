## Mini Nginx

### 运行方式

1、在项目的根目录下新建 build 文件夹

2、进入 build 文件夹并且编译

```shell
cd /path/to/build
cmake .
cmake --build .
```

3、配置conf文件，参考项目解析中给出的配置文件

4、运行项目（注意运行权限问题）

```shell
./Mini-Nginx
```



### 项目解析

 #### HTTP 报文解析

```http
Request URL: http://116.62.7.191/test/weightlunxun/nihao
Request Method: GET

Request Header:
Host: localhost
User-Agent: Mozilla/5.0 (Windows NT 10.0; Wisn64; x64) AppleWebKit/537.36 (KHTML,like Gecko) Chrome/77.0.3865.90Safari/537.36
Accept: */*
```

对于自定义形式的报文，采用如下的方式存储

```C++
struct HttpRequest
{
    std::string method;
    std::string url;
    std::map<std::string, std::string> headers;
};

```

HTTP自定义报文解析器

```c++
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
```

辅助函数

```c++
// 从 ip:port中提取出 ip 和 port
void ExtractIpPort(const std::string &ipPortStr, std::string &ip, int &port);

// 从 proxy_set_header 中提取键值对
void ExtractHeader(std::map<std::string, std::string> &Headers, std::string proxy_set_header);
```



#### 反向代理

```nginx
location = /test {
            proxy_pass 127.0.0.1:8000;
            proxy_set_header Host test.com;
        }

```

反向代理类

```c++
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
```



#### 静态资源代理

```nginx
location /static {
            root /home/cxj;
            index index.html;
        }
```

静态资源代理类

```c++
class StaticResourcesProxy
{
public:
    StaticResourcesProxy(const std::string FilePath);

    // 处理客户端文件请求信息
    void StaticResourcesProxyRequest(int ClientSocket, ServerBlock Server, HttpRequest requestFromClient);

private:
    const std::string FilePath_;
};
```



#### 对静态资源代理和反向代理进行管理

```c++
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
```



#### Nginx 的 conf 配置文件解析

conf 文件展示

```nginx
# worker 进程数量
worker_processes auto;

http {
    # 负载均衡
    upstream backend1 {
        server 127.0.0.1:8000;
        server 127.0.0.1:8001;
        server 127.0.0.1:8002;
    }

    upstream backend2 {
        ip_hash;
        server 127.0.0.1:8000;
        server 127.0.0.1:8001;
        server 127.0.0.1:8002;
    }

    upstream backend3 {
        server 127.0.0.1:8000 weight=2;
        server 127.0.0.1:8001 weight=7;
        server 127.0.0.1:8002 weight=1;
    }
    
    server {
        listen 80;
        server_name 116.62.7.191;

        error_log /home/cxj/log/error.log;
        access_log /home/cxj/log/access.log;

        location = /test {
            proxy_pass http://backend1;
            proxy_set_header Host test.com;
        }

        location ^~ /test/iphash {
            proxy_pass http://backend2;
            proxy_set_header Host test.com;
        }

        location  /test/weightlunxun {
            proxy_pass http://backend3;
            proxy_set_header Host test.com;
        }

        location /static {
            root /home/cxj;
            index index.html;
        }
    }
}
```

存储的数据结构

```c++
enum Method
{
    RoundRobin,
    WeightedRoundRobin,
    IPHash
};

struct LocationBlock
{
    std::string path;
    std::string proxy_pass;
    std::string proxy_set_header;
    std::string root;
    std::string index;

    LocationBlock()
        : path(""), proxy_pass(""), proxy_set_header(""), root(""), index("index.html") {}
};

struct ServerBlock
{
    std::string listen;
    std::vector<std::string> server_names;
    std::string access_log;
    std::string error_log;
    std::vector<LocationBlock> locations;

    ServerBlock() 
        : listen("80"), access_log("/home/cxj/log/access.log"), error_log("/home/cxj/log/error.log") {}
};

struct upstream
{
    std::string host;
    std::vector<std::string> servers;
    Method method;

    upstream()
        : method(RoundRobin) {}
};

struct http
{
    std::vector<ServerBlock> Servers;
    std::vector<upstream> Upstreams;
    int NumWorkers;

    http()
        : NumWorkers(4) {}
};
```



#### 匹配规则

server_name

```c++
// Server 规则
void MatchServer(int ClientSocket, const char *buffer);

// 精确匹配
bool MatchServerNameExact(const std::string &pattern, const std::string &HostName);

// 通配符匹配
bool MatchServerNameWildcard(const std::string &pattern, const std::string &HostName);

// 正则表达式匹配
bool MatchServerNameRegex(const std::string &pattern, const std::string &HostName);
```

location

```c++
// 精确匹配 > 前缀匹配 > 正则匹配 > 通用匹配 > 万能匹配

void MatchLocation(int ClientSocket, HttpRequest Request, ServerBlock Server, HttpRequestParser parser);
```



#### 日志类

```c++
enum LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger
{
public:
    Logger() {}
    Logger(const std::string &LogFile);

    ~Logger();

    // 自定义赋值运算符重载
    Logger &operator=(Logger &other) noexcept
    {
        if (this != &other)
        { // 检查是否尝试自我赋值
            // 在赋值前，首先释放本对象的资源（如果有的话）
            CloseResource(); // 假设这是关闭资源的函数

            // 然后进行资源的移动
            LogFileStream_ = std::move(other.LogFileStream_); // 假设 LogFileStream_ 是需要转移的资源
            LogLevel_ = other.LogLevel_;

            // 清空 other 对象，确保它不再引用资源
            other.LogFileStream_.close(); // 关闭文件（如果已打开）
            other.LogLevel_ = INFO;       // 重置 LogLevel
        }
        return *this;
    }

    void LogAcess(LogLevel Level, const HttpRequest Request, const char *Response);

    void LogError(LogLevel Level, std::string ErrorInfo);

private:
    std::ofstream LogFileStream_;
    LogLevel LogLevel_ = INFO; // 默认日志级别为INFO
    std::mutex LogMutex_;

    // 辅助函数，用于释放资源
    void CloseResource()
    {
        LogFileStream_.close(); // 关闭文件（如果已打开）
    }
};
```



#### 负载均衡

支持轮询、加权轮询、IP哈希三种负载均衡

```nginx
upstream backend1 {
    server 127.0.0.1:8000;
    server 127.0.0.1:8001;
    server 127.0.0.1:8002;
}

upstream backend2 {
    ip_hash;
    server 127.0.0.1:8000;
    server 127.0.0.1:8001;
    server 127.0.0.1:8002;
}

upstream backend3 {
    server 127.0.0.1:8000 weight=2;
    server 127.0.0.1:8001 weight=7;
    server 127.0.0.1:8002 weight=1;
}
```



#### 附加功能

Master 和 Woker 进程的分离、epoll事务的并发

```nginx
worker_processes auto;
```

