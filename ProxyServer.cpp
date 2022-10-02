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

QPair<SOCKET,QString> ConnectToServer(const char* host)
{
	SOCKADDR_IN to_server_socket_addr;
	to_server_socket_addr.sin_family = AF_INET;
	// http端口使用80
	to_server_socket_addr.sin_port = htons(80);

	// 查询域名对应的主机地址
	HOSTENT* hostent = gethostbyname(host);
	if (hostent == nullptr)
	{
		return { INVALID_SOCKET,
			QString("gethostbyname()函数返回NULL, 错误码为%1")
			.arg(WSAGetLastError()) };
	}

	IN_ADDR server_addr = *reinterpret_cast<IN_ADDR*>(hostent->h_addr_list[0]);
	to_server_socket_addr.sin_addr.S_un.S_addr = inet_addr(inet_ntoa(server_addr));

	SOCKET to_server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (to_server_socket == INVALID_SOCKET)
	{
		return { INVALID_SOCKET,"创建到服务器的套接字失败"};
	}

	if (connect(to_server_socket,
		reinterpret_cast<SOCKADDR*>(&to_server_socket_addr),
		sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		closesocket(to_server_socket);
		return { INVALID_SOCKET,
			QString("connect()函数调用错误, 错误码为%1")
			.arg(WSAGetLastError()) };
	}

	return { to_server_socket,"" };
}

void ProxyServer::Start(u_short port)
{
	const char* kErrorPrefix = "【套接字初始化错误】: ";

	int status = 0;

	WSADATA wsaData;
	status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0)
	{
		// 由于还没有一次对WSAStartup的成功调用，WSACleanup不应在此被调用
		qCritical() << kErrorPrefix
			<< QString("WSAStartup()调用失败，返回值为%1").arg(status);
		std::exit(-1);
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		ERR_EXIT(kErrorPrefix,
			QString("未预期的Windows Sockets版本: %1.%2")
				.arg(LOBYTE(wsaData.wVersion))
				.arg(HIBYTE(wsaData.wVersion)));
	}

	// 处理HTTP请求使用的是TCP协议，此代理服务器使用IPv4协议，
	// 因此指定参数为AF_INET(IPv4)和SOCK_STREAM(TCP)
	this->server_socket_ = socket(AF_INET, SOCK_STREAM, 0);

	if (this->server_socket_ == INVALID_SOCKET)
	{
		ERR_EXIT(kErrorPrefix,"创建到客户端的套接字失败");
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
			QString("bind()调用失败，返回值为%1").arg(status));
	}

	status = listen(this->server_socket_, SOMAXCONN);
	if (status != 0)
	{
		ERR_EXIT(kErrorPrefix,
			QString("listen()调用失败，返回值为%1.").arg(status));
	}

	// 设置键盘事件处理程序
	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	qInfo()<<QString(
		"【代理服务器启动成功】监听端口: %1")
		.arg(port);

	this->RunServiceLoop();
}

void ProxyServer::RunServiceLoop() const
{
	const char* kErrorPrefix = "【服务失败】: ";

	while (true)
	{
		auto to_client_socket = accept(this->server_socket_, nullptr, nullptr);

		std::thread service_thread([kErrorPrefix, to_client_socket]
			{
				// 1MB
				constexpr int kBufferSize = 1*1024*1024;

				QByteArray buffer(kBufferSize, '\0');

				// 接收客户端请求数据
				int status = recv(to_client_socket, buffer.data(), kBufferSize, 0);
				
				// 实验证明status储存的数据长度不包含结尾'\0'
				int client_recv_size = status;

				if (status == SOCKET_ERROR)
				{
					STOP_SERVICE_CLOSE1(to_client_socket,
						kErrorPrefix,
						QString("接收客户端请求数据时recv()调用失败，返回值为%1")
							.arg(WSAGetLastError()));
				}

				// 构建请求头对象，包含请求关键信息
				HttpHeader header(buffer);

				// 打印请求信息
				qDebug() << QString("[%1] %2 %3 请求主机:%4 请求数据长度:%5 时间:%6")
					.arg(header.method())
					.arg(header.url())
					.arg(header.http_version())
					.arg(header.host())
					.arg(client_recv_size)
					.arg(QDateTime::currentDateTime()
						.toString("yyyy/MM/dd hh:mm:ss"));

				// 仅处理GET/POST/PUT/DELETE请求
				if (header.method() != "GET" 
					&& header.method() != "POST"
					&& header.method() != "PUT"
					&& header.method() != "DELETE")
				{
					STOP_SERVICE_CLOSE1(to_client_socket,
						"[忽略请求]: ",
						QString("不能处理的请求方法: %1")
							.arg(header.method()));
				}

				// 连接到客户端请求的服务器
				auto connect_result = ConnectToServer(header.host().toUtf8());

				SOCKET to_server_socket = connect_result.first;

				if (to_server_socket == INVALID_SOCKET)
				{
					STOP_SERVICE_CLOSE1(to_client_socket,
						kErrorPrefix,
						connect_result.second);
				}

				// 向请求服务器发送请求
					// 同样，长度信息无需包含结尾'\0'
				status = send(to_server_socket,
					buffer.constData(),
					client_recv_size,
					0);

				if (status == SOCKET_ERROR)
				{
					STOP_SERVICE_CLOSE2(to_server_socket,
						to_client_socket,
						kErrorPrefix,
						QString("向服务器发送请求时send()调用失败，返回值为%1")
						.arg(WSAGetLastError()));
				}

				// 逐个接收所有响应数据块，逐个发回客户端
				while (true)
				{
					status = recv(to_server_socket, buffer.data(), kBufferSize, 0);

					int server_recv_size = status;

					if (server_recv_size == 0)
					{
						break;
					}

					if (status == SOCKET_ERROR)
					{
						STOP_SERVICE_CLOSE2(to_server_socket,
							to_client_socket,
							kErrorPrefix,
							QString(
								"接收服务器响应数据时recv()调用失败，返回值为%1")
							.arg(WSAGetLastError()));
					}

					qDebug() << QString("成功收到响应数据 响应数据长度:%1")
						.arg(server_recv_size);

					// 将数据发回客户端
					status = send(to_client_socket,
						buffer.constData(),
						server_recv_size,
						0);

					if (status == SOCKET_ERROR)
					{
						STOP_SERVICE_CLOSE2(to_server_socket,
							to_client_socket,
							kErrorPrefix,
							QString(
								"向客户端发回数据时send()调用失败，返回值为%1")
							.arg(WSAGetLastError()));
					}
				}

				closesocket(to_server_socket);
				closesocket(to_client_socket);
			});

		service_thread.detach();

		Sleep(30);
	}
}
