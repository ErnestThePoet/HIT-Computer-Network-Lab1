#include "ProxyServer.h"

BOOL WINAPI CtrlHandler(DWORD type)
{
	switch (type)
	{
	case CTRL_BREAK_EVENT:
		WSACleanup();
		std::exit(0);

	default:
		return FALSE;
	}
}

void ProxyServer::Start(u_short port)
{
	const char* kErrorPrefix = "Socket Initialization Error: ";

	int status = 0;

	WSADATA wsaData;
	status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0)
	{
		// 由于还没有一次对WSAStartup的成功调用，WSACleanup不应在此被调用
		qCritical() << kErrorPrefix
			<< QString("WSAStartup() failed with return code %1.").arg(status);
		std::exit(-1);
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		ERR_EXIT(kErrorPrefix,
			QString("Unexpected Windows Sockets version: %1.%2")
				.arg(LOBYTE(wsaData.wVersion))
				.arg(HIBYTE(wsaData.wVersion)));
	}

	// 处理HTTP请求使用的是TCP协议，此代理服务器使用IPv4协议，
	// 因此指定参数为AF_INET(IPv4)和SOCK_STREAM(TCP)
	this->server_socket_ = socket(AF_INET, SOCK_STREAM, 0);

	if (this->server_socket_ == INVALID_SOCKET)
	{
		ERR_EXIT(kErrorPrefix,"Failed to create socket.");
	}

	SOCKADDR_IN server_socket_addr;
	server_socket_addr.sin_family = AF_INET;
	server_socket_addr.sin_port = htons(port);
	// 使用任意地址“通配符”
	server_socket_addr.sin_addr.S_un.S_addr = INADDR_ANY;

	status = bind(this->server_socket_, 
		reinterpret_cast<SOCKADDR*>(&server_socket_addr),
		sizeof(SOCKADDR));
	if (status != 0)
	{
		ERR_EXIT(kErrorPrefix,
			QString("bind() failed with return code %1.").arg(status));
	}

	status = listen(this->server_socket_, SOMAXCONN);
	if (status != 0)
	{
		ERR_EXIT(kErrorPrefix,
			QString("listen() failed with return code %1.").arg(status));
	}

	// 设置键盘事件处理程序
	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	qInfo()<<QString(
		"Successfully started proxy server. Listening at port %1.")
		.arg(port);

	this->RunServiceLoop();
}

void ProxyServer::RunServiceLoop() const
{
	const char* kErrorPrefix = "Server Error: ";

	while (true)
	{
		auto to_client_socket = accept(this->server_socket_, nullptr, nullptr);

		std::thread service_thread([kErrorPrefix, to_client_socket]
			{
				constexpr int kBufferSize = 65536;

				std::vector<char> buffer(kBufferSize);

				int status = recv(to_client_socket, buffer.data(), kBufferSize, 0);

				if (status == SOCKET_ERROR)
				{
					qCritical()
						<< kErrorPrefix
						<< QString(
							"recv() failed with error code %1.\n")
								.arg(WSAGetLastError());

					return;
				}

				closesocket(to_client_socket);
			});

		service_thread.detach();

		Sleep(200);
	}
}