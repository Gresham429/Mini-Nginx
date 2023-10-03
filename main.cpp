#include "http_proxy.h"
#include <iostream>

int main()
{
    HttpProxy ProxyServer(80);
    ProxyServer.Start();

    return 0;
}