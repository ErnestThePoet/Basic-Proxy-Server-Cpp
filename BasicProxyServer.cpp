#include "BasicProxyServer.h"

std::pair<SOCKET, std::string> ConnectToServer(const char* host, const u_short port)
{
	SOCKADDR_IN sock_addr;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(port);

	// 查询域名对应的主机地址
	HOSTENT* hostent = gethostbyname(host);
	if (hostent == nullptr)
	{
		return { INVALID_SOCKET,
			std::string("gethostbyname调用错误, 错误代码")+
			std::to_string(WSAGetLastError()) };
	}

	IN_ADDR server_addr = *reinterpret_cast<IN_ADDR*>(hostent->h_addr_list[0]);
	sock_addr.sin_addr.S_un.S_addr = inet_addr(inet_ntoa(server_addr));

	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == INVALID_SOCKET)
	{
		return { INVALID_SOCKET,"创建到服务器的套接字失败" };
	}

	if (connect(server_socket,
		reinterpret_cast<SOCKADDR*>(&sock_addr),
		sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		closesocket(server_socket);
		return { INVALID_SOCKET,
			std::string("connect调用错误, 错误代码")
			+std::to_string(WSAGetLastError()) };
	}

	return { server_socket,"" };
}

void BasicProxyServer::StartAtPort(u_short port)
{
	int err = 0;

	WSADATA wsaData;
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0)
	{
		// 由于还没有一次对WSAStartup的成功调用，WSACleanup不应在此被调用
		std::cerr << std::string("WSAStartup调用错误，错误代码")+std::to_string(err);
		std::exit(-1);
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		ERROR_EXIT("socket版本错误");
	}

	// 处理HTTP请求使用的是TCP协议，此代理服务器使用IPv4协议，
	// 因此指定参数为AF_INET(IPv4)和SOCK_STREAM(TCP)
	this->proxy_socket_ = socket(AF_INET, SOCK_STREAM, 0);

	if (this->proxy_socket_ == INVALID_SOCKET)
	{
		ERROR_EXIT("创建到客户端的套接字失败");
	}

	SOCKADDR_IN sock_addr;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(port);
	sock_addr.sin_addr.S_un.S_addr = INADDR_ANY;

	err = bind(this->proxy_socket_,
		reinterpret_cast<SOCKADDR*>(&sock_addr),
		sizeof(SOCKADDR));
	if (err != 0)
	{
		ERROR_EXIT(
			std::string("bind调用错误，错误代码")+std::to_string(err));
	}

	err = listen(this->proxy_socket_, SOMAXCONN);
	if (err != 0)
	{
		ERROR_EXIT(
			std::string("listen调用错误，错误代码")+std::to_string(err));
	}

	std::cout << std::string(
		"【代理服务器运行】端口:")
		+std::to_string(port);

	this->RunServiceLoop();
}

void BasicProxyServer::RunServiceLoop() const
{
	while (true)
	{
		auto client_socket = accept(this->proxy_socket_,nullptr,nullptr);

		std::thread service_thread([client_socket]
			{
				// 1MB
				constexpr int kBufferSize = 1 * 1024 * 1024;

				std::vector<char> buffer(kBufferSize, '\0');

				// 接收客户端请求数据
				int err = recv(client_socket, buffer.data(), kBufferSize, 0);

				int client_recv_size = err;

				if (err == SOCKET_ERROR)
				{
					STOP_CLOSE_1(client_socket,
						std::string("接收请求数据时recv调用错误，错误代码")
						+std::to_string(WSAGetLastError()));
				}

				// 解析请求头
				HttpHeader header(buffer);

				if (header.method() == "GET"
					|| header.method() == "POST")
				{
					std::cout << "收到客户端请求 请求方法:" 
						<< header.method() 
						<< " 请求url:" 
						<< header.url() 
						<< " 请求主机: " 
						<< header.host() 
						<< std::endl;
				}
				else
				{
					closesocket(client_socket);
					return;
				}

				// 连接到客户端请求的服务器，端口80
				auto connect_result =
					ConnectToServer(header.host().c_str(), 80);

				SOCKET server_socket = connect_result.first;

				if (server_socket == INVALID_SOCKET)
				{
					STOP_CLOSE_1(client_socket,
						connect_result.second);
				}

				// 转发请求
				err = send(server_socket,
					buffer.data(),
					client_recv_size + 1,
					0);

				if (err == SOCKET_ERROR)
				{
					STOP_CLOSE_2(server_socket,
						client_socket,
						std::string("向服务器发送请求时send()调用错误，错误代码")
						+std::to_string(WSAGetLastError()));
				}

				// 接收所有响应数据
				while (true)
				{
					err = recv(server_socket, buffer.data(), kBufferSize, 0);

					int server_recv_size = err;

					if (server_recv_size == 0)
					{
						break;
					}

					if (err == SOCKET_ERROR)
					{
						STOP_CLOSE_2(server_socket,
							client_socket,
							std::string(
								"接收响应数据时recv调用错误，错误代码")
							+std::to_string(WSAGetLastError()));
					}

					std::cout << "收到响应数据，长度为" << server_recv_size<<std::endl;

					// 代理发回客户端
					err = send(client_socket,
						buffer.data(),
						server_recv_size,
						0);

					if (err == SOCKET_ERROR)
					{
						STOP_CLOSE_2(server_socket,
							client_socket,
							std::string(
								"发回数据时send调用错误，错误代码")
							+std::to_string(WSAGetLastError()));
					}
				}

				closesocket(server_socket);
				closesocket(client_socket);
			});

		service_thread.detach();

		Sleep(50);
	}
}
