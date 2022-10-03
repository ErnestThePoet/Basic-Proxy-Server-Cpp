#pragma once

#include <Windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <utility>

#include "HttpHeader.h"

#pragma comment(lib,"ws2_32.lib")

#define ERROR_EXIT(M) \
	do{WSACleanup();std::cerr<<(M)<<std::endl;std::exit(-1);}while(false)
#define STOP_CLOSE_1(S,M) \
	do{closesocket(S);std::cout<<(M)<<std::endl;return;}while(false)
#define STOP_CLOSE_2(S1,S2,M) \
	do{closesocket(S1);closesocket(S2);std::cout<<(M)<<std::endl;return;}while(false)

class BasicProxyServer
{
private:
	SOCKET proxy_socket_ = INVALID_SOCKET;

	void RunServiceLoop() const;
public:
	BasicProxyServer() = default;

	~BasicProxyServer()
	{
		WSACleanup();
	}

	void StartAtPort(u_short port);
};

