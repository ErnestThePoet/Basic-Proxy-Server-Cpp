#include "BasicProxyServer.h"

std::pair<SOCKET, std::string> ConnectToServer(const char* host, const u_short port)
{
	SOCKADDR_IN sock_addr;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(port);

	// ��ѯ������Ӧ��������ַ
	HOSTENT* hostent = gethostbyname(host);
	if (hostent == nullptr)
	{
		return { INVALID_SOCKET,
			std::string("gethostbyname���ô���, �������")+
			std::to_string(WSAGetLastError()) };
	}

	IN_ADDR server_addr = *reinterpret_cast<IN_ADDR*>(hostent->h_addr_list[0]);
	sock_addr.sin_addr.S_un.S_addr = inet_addr(inet_ntoa(server_addr));

	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == INVALID_SOCKET)
	{
		return { INVALID_SOCKET,"���������������׽���ʧ��" };
	}

	if (connect(server_socket,
		reinterpret_cast<SOCKADDR*>(&sock_addr),
		sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		closesocket(server_socket);
		return { INVALID_SOCKET,
			std::string("connect���ô���, �������")
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
		// ���ڻ�û��һ�ζ�WSAStartup�ĳɹ����ã�WSACleanup��Ӧ�ڴ˱�����
		std::cerr << std::string("WSAStartup���ô��󣬴������")+std::to_string(err);
		std::exit(-1);
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		ERROR_EXIT("socket�汾����");
	}

	// ����HTTP����ʹ�õ���TCPЭ�飬�˴��������ʹ��IPv4Э�飬
	// ���ָ������ΪAF_INET(IPv4)��SOCK_STREAM(TCP)
	this->proxy_socket_ = socket(AF_INET, SOCK_STREAM, 0);

	if (this->proxy_socket_ == INVALID_SOCKET)
	{
		ERROR_EXIT("�������ͻ��˵��׽���ʧ��");
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
			std::string("bind���ô��󣬴������")+std::to_string(err));
	}

	err = listen(this->proxy_socket_, SOMAXCONN);
	if (err != 0)
	{
		ERROR_EXIT(
			std::string("listen���ô��󣬴������")+std::to_string(err));
	}

	std::cout << std::string(
		"��������������С��˿�:")
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

				// ���տͻ�����������
				int err = recv(client_socket, buffer.data(), kBufferSize, 0);

				int client_recv_size = err;

				if (err == SOCKET_ERROR)
				{
					STOP_CLOSE_1(client_socket,
						std::string("������������ʱrecv���ô��󣬴������")
						+std::to_string(WSAGetLastError()));
				}

				// ��������ͷ
				HttpHeader header(buffer);

				if (header.method() == "GET"
					|| header.method() == "POST")
				{
					std::cout << "�յ��ͻ������� ���󷽷�:" 
						<< header.method() 
						<< " ����url:" 
						<< header.url() 
						<< " ��������: " 
						<< header.host() 
						<< std::endl;
				}
				else
				{
					closesocket(client_socket);
					return;
				}

				// ���ӵ��ͻ�������ķ��������˿�80
				auto connect_result =
					ConnectToServer(header.host().c_str(), 80);

				SOCKET server_socket = connect_result.first;

				if (server_socket == INVALID_SOCKET)
				{
					STOP_CLOSE_1(client_socket,
						connect_result.second);
				}

				// ת������
				err = send(server_socket,
					buffer.data(),
					client_recv_size + 1,
					0);

				if (err == SOCKET_ERROR)
				{
					STOP_CLOSE_2(server_socket,
						client_socket,
						std::string("���������������ʱsend()���ô��󣬴������")
						+std::to_string(WSAGetLastError()));
				}

				// ����������Ӧ����
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
								"������Ӧ����ʱrecv���ô��󣬴������")
							+std::to_string(WSAGetLastError()));
					}

					std::cout << "�յ���Ӧ���ݣ�����Ϊ" << server_recv_size<<std::endl;

					// �����ؿͻ���
					err = send(client_socket,
						buffer.data(),
						server_recv_size,
						0);

					if (err == SOCKET_ERROR)
					{
						STOP_CLOSE_2(server_socket,
							client_socket,
							std::string(
								"��������ʱsend���ô��󣬴������")
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
