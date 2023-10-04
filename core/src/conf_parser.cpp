#include "conf_parser.h"
#include "http_parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>

// 解析配置文件
std::vector<ServerBlock> ParserConf(const std::string &FileName)
{
    std::vector<ServerBlock> ServerBlocks;
    ServerBlock CurrentServerBlock;
    LocationBlock CurrentLocationBlock;

    std::ifstream ConfigFile(FileName);
    if (!ConfigFile.is_open())
    {
        std::cerr << "Failed to open config file: " << FileName << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;
    bool InsideLocationBlock = false;
    bool InsideServerBlock = false;
    while (std::getline(ConfigFile, line))
    {
        // 去除首尾空格
        line = std::regex_replace(line, std::regex("^\\s+|\\s+$"), "");

        // 去除结尾的分号
        if (!line.empty() && line.back() == ';')
        {
            line.pop_back();
        }

        // 忽略注释和空行
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::regex LocationRegex("location\\s+(.*?)\\s*\\{");
        std::smatch LocationMatch;
        // 如果包含左花括号 '{'，说明进入了一个配置块
        if (line.find("{") != std::string::npos)
        {
            if (line == "http {")
            {
                continue; // 忽略 http 块
            }
            else if (line == "server {")
            {
                CurrentServerBlock = ServerBlock();
                InsideServerBlock = true;
            }
            else if (std::regex_match(line, LocationMatch, LocationRegex))
            {
                CurrentLocationBlock = LocationBlock();
                CurrentLocationBlock.path = LocationMatch[1];
                InsideLocationBlock = true;
            }
            continue;
        }

        // 处理配置语句
        if (line.find("listen") == 0)
        {
            CurrentServerBlock.listen = line.substr(7); // 去掉 "listen " 部分
        }
        else if (line.find("server_name") == 0)
        {
            std::string names = line.substr(12); // 去掉 "server_name " 部分
            std::istringstream ss(names);
            std::string ServerName;
            while (std::getline(ss, ServerName, ' ') && !ServerName.empty())
            {
                CurrentServerBlock.server_names.push_back(ServerName);
            }
        }
        else if (line.find("access_log") == 0)
        {
            CurrentServerBlock.access_log = line.substr(11); // 去掉 "access_log " 部分
        }
        else if (line.find("error_log") == 0)
        {
            CurrentServerBlock.error_log = line.substr(10); // 去掉 "error_log " 部分
        }
        else if (InsideLocationBlock)
        {
            if (line.find("proxy_pass") == 0)
            {
                CurrentLocationBlock.proxy_pass = line.substr(11); // 去掉 "proxy_pass " 部分
            }
            else if (line.find("proxy_set_header") == 0)
            {
                CurrentLocationBlock.proxy_set_header = line.substr(17); // 去掉 "root " 部分
            }
            else if (line.find("root") == 0)
            {
                CurrentLocationBlock.root = line.substr(5); // 去掉 "root " 部分
            }
            else if (line.find("index") == 0)
            {
                CurrentLocationBlock.index = line.substr(6); // 去掉 "index " 部分
            }
        }

        // 如果包含右花括号 '}'，说明结束了一个配置块
        if (line == "}")
        {
            if (InsideServerBlock && InsideLocationBlock)
            {
                CurrentServerBlock.locations.push_back(CurrentLocationBlock);
                InsideLocationBlock = false;
            }
            else if (InsideServerBlock && !InsideLocationBlock)
            {
                ServerBlocks.push_back(CurrentServerBlock);
                InsideServerBlock = false;
            }
        }
    }

    ConfigFile.close();

    return ServerBlocks;
}