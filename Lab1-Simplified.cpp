#include "BasicProxyServer.h"

int main()
{
    BasicProxyServer server;
    // 在这里指定监听端口
    server.StartAtPort(11750);

    return 0;
}
