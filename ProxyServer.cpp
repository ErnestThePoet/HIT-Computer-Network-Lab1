#include "ProxyServer.h"

void Log(const std::string& message)
{
	std::cout << message << std::endl;
}

BOOL WINAPI CtrlHandler(DWORD type)
{
	switch (type)
	{
	case CTRL_C_EVENT:
		std::cout << "123";
		WSACleanup();
		std::exit(0);

	default:
		return FALSE;
	}
}

void ProxyServer::Start()
{
	const char* kErrorPrefix = "Socket Initialization Error: ";

	int status = 0;

	WSADATA wsaData;
	status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0)
	{
		// 由于还没有一次对WSAStartup的成功调用，WSACleanup不应在此被调用
		std::cerr << kErrorPrefix
			<< std::format("WSAStartup() failed with return code {:d}.", status);
		std::exit(-1);
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		ERR_EXIT(kErrorPrefix,
			std::format("Unexpected Windows Sockets version: {:d} {:d}",
				LOBYTE(wsaData.wVersion),
				HIBYTE(wsaData.wVersion)));
	}

	// 处理HTTP请求使用的是TCP协议，此代理服务器使用IPv4协议，
	// 因此指定参数为AF_INET(IPv4)和SOCK_STREAM(TCP)
	this->server_socket_ = socket(AF_INET, SOCK_STREAM, 0);

	if (this->server_socket_ == INVALID_SOCKET)
	{
		ERR_EXIT(kErrorPrefix,
			std::format("Failed to create socket."));
	}

	this->server_socket_addr_.sin_family = AF_INET;
	this->server_socket_addr_.sin_port = htons(this->port_);
	// 使用任意地址“通配符”
	this->server_socket_addr_.sin_addr.S_un.S_addr = INADDR_ANY;

	status = bind(this->server_socket_, 
		reinterpret_cast<SOCKADDR*>(&this->server_socket_addr_),
		sizeof(SOCKADDR));
	if (status != 0)
	{
		ERR_EXIT(kErrorPrefix,
			std::format("bind() failed with return code {:d}.", status));
	}

	status = listen(this->server_socket_, SOMAXCONN);
	if (status != 0)
	{
		ERR_EXIT(kErrorPrefix,
			std::format("listen() failed with return code {:d}.", status));
	}

	// 设置键盘事件处理程序
	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	Log(std::format(
		"Successfully started proxy server. Listening at port {:d}.", this->port_));

	this->RunServiceLoop();
}

void ProxyServer::RunServiceLoop() const
{
	const char* kErrorPrefix = "Server Error: ";

	while (true)
	{
		auto request_socket = accept(this->server_socket_, nullptr, nullptr);

		std::thread service_thread([kErrorPrefix, request_socket]
			{
				constexpr int kBufferSize = 65536;

				std::vector<char> buffer(kBufferSize);

				int status = recv(request_socket, buffer.data(), kBufferSize, 0);

				if (status == SOCKET_ERROR)
				{
					std::cerr
						<< kErrorPrefix
						<< std::format(
							"recv() failed with error code {:d}.",
							WSAGetLastError())
						<< std::endl;

					return;
				}

				std::cout << buffer.data() << std::endl;

				closesocket(request_socket);
			});

		service_thread.detach();

		Sleep(200);
	}
}