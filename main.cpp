#include "http_proxy.h"
#include "conf_parser.h"
#include "log.h"
#include <iostream>
#include <vector>
#include <map>

std::map<std::string, Logger> LogMap;

int main()
{
    std::string filename;
    std::cout << "请输入配置文件路径：";
    std::cin >> filename;

    http HttpServers = ParserConf(filename);

    std::map<std::string, std::vector<ServerBlock>> ServerMap;

    for (const auto &Server : HttpServers.Servers)
    {
        // 端口归类
        auto it = ServerMap.find(Server.listen);
        if (it != ServerMap.end())
        {
            it->second.push_back(Server);
        }
        else
        {
            ServerMap[Server.listen] = {Server};
        }

        // 日志文件归类
        auto access_it = LogMap.find(Server.access_log);
        if (access_it == LogMap.end())
        {
            Logger AccessLogger(Server.access_log);
            LogMap[Server.access_log] = AccessLogger;
        }
        auto error_it = LogMap.find(Server.error_log);
        if (error_it == LogMap.end())
        {
            Logger ErrorLogger(Server.error_log);
            LogMap[Server.error_log] = ErrorLogger;
        }
    }

    HttpProxy HttpProxy_(HttpServers.NumWorkers, ServerMap, HttpServers.Upstreams);
    HttpProxy_.Start();

    return 0;
}