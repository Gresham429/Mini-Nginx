#ifndef CONF_PARSER_H
#define CONF_PARSER_H

#include <string>
#include <vector>

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
};

http ParserConf(const std::string &FileName);

#endif