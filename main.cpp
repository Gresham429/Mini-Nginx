#include "http_proxy.h"
#include "conf_parser.h"
#include <iostream>
#include <vector>
#include <map>

int main()
{
    std::vector<ServerBlock> Servers = ParserConf("/home/cxj/MiniNginx.conf");

    std::map<std::string, std::vector<ServerBlock>> ServerMap;

    for (const auto &Server : Servers)
    {
        auto it = ServerMap.find(Server.listen);
        if (it != ServerMap.end())
        {
            it->second.push_back(Server);
        }
        else
        {
            ServerMap[Server.listen] = {Server};
        }
    }

    HttpProxy HttpProxy_(ServerMap);
    HttpProxy_.Start();

    return 0;
}