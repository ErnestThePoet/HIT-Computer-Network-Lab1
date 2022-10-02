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

QPair<SOCKET,QString> ConnectToServer(const char* host,const u_short port)
{
	SOCKADDR_IN to_server_socket_addr;
	to_server_socket_addr.sin_family = AF_INET;
	to_server_socket_addr.sin_port = htons(port);

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

	CacheManager::LoadFromDisk();
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

				// 仅处理GET/POST/PUT/DELETE请求
				if (header.method() == "GET" 
					|| header.method() == "POST"
					|| header.method() == "PUT"
					|| header.method() == "DELETE")
				{
					// 打印请求信息
					qInfo() << QString("[%1] %2 %3 请求主机:%4 请求数据长度:%5 时间:%6")
						.arg(header.method())
						.arg(header.url())
						.arg(header.http_version())
						.arg(header.host())
						.arg(client_recv_size)
						.arg(QDateTime::currentDateTime()
							.toString("yyyy/MM/dd hh:mm:ss"));
				}
				else
				{
					closesocket(to_client_socket);
					return;
				}

				// 连接到客户端请求的服务器
				// http端口使用80
				auto connect_result = 
					ConnectToServer(header.host().toUtf8(),80);

				SOCKET to_server_socket = connect_result.first;

				if (to_server_socket == INVALID_SOCKET)
				{
					STOP_SERVICE_CLOSE1(to_client_socket,
						kErrorPrefix,
						connect_result.second);
				}

				// 查询缓存中是否有该url请求结果的缓存
				auto cache_query_result = CacheManager::QueryCache(header.url());

				// 如有缓存，向服务器发送包含If-Modified-Since字段的请求，
				// 查询缓存是否为最新
				if (cache_query_result.is_existent)
				{
					// 如果原请求头包含If-Modified-Since，那么将其替换；
					// 否则插入一条字段
					int key_index = buffer.indexOf("If-Modified-Since:");
					if (key_index!=-1)
					{
						int val_end_index = buffer.indexOf("\r\n", key_index);

						buffer.replace(key_index, val_end_index - key_index,
							QString("If-Modified-Since: %1")
							.arg(cache_query_result.last_modified).toUtf8());
					}
					else
					{
						buffer.insert(buffer.indexOf("\r\n") + 2,
							QString("If-Modified-Since: %1\r\n")
							.arg(cache_query_result.last_modified).toUtf8());
					}

					qInfo() << QString("[缓存命中] %1 已向请求头插入If-Modified-Since字段")
						.arg(header.url());
				}

				// 向请求服务器发送请求
				// 实验证明，长度参数需要包含结尾'\0'，否则服务器将不响应某些请求
				status = send(to_server_socket,
					buffer.constData(),
					client_recv_size+1,
					0);

				if (status == SOCKET_ERROR)
				{
					STOP_SERVICE_CLOSE2(to_server_socket,
						to_client_socket,
						kErrorPrefix,
						QString("向服务器发送请求时send()调用失败，返回值为%1")
						.arg(WSAGetLastError()));
				}

				// 对于缓存未命中的请求，如果响应包含Lat-Modified字段才应该进行缓存
				bool should_cache = false;
				int cache_id = -1;

				// 逐个接收所有响应数据块，逐个发回客户端
				for(int i=0;;i++)
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

					// 如果之前缓存命中且响应码为304，那么直接分块返回缓存数据
					if (i == 0 
						&& cache_query_result.is_existent
						&& buffer.split('\r')[0].contains("304"))
					{
						qInfo() << QString("[304 Not Modified] %1 将发回缓存数据")
							.arg(header.url());
						auto cache_chunks = CacheManager::ReadCacheChunks(
							cache_query_result.cache_id);

						for (auto j : cache_chunks)
						{
							status = send(to_client_socket,
								j.constData(),
								j.size(),
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

						break;
					}

					// 如果缓存未命中或已失效，那么存入缓存
					if (i == 0)
					{
						QString response_str(buffer);
						auto response_splitted = response_str.split("\r\n");
						for (auto& j : response_splitted)
						{
							if (j.startsWith("Last-Modified: ", Qt::CaseInsensitive))
							{
								cache_id=CacheManager::CreateCache(
									header.url(),
									j.remove("Last-Modified: ", Qt::CaseInsensitive));

								should_cache = true;
								break;
							}
						}
					}

					if (should_cache)
					{
						CacheManager::AppendCacheChunk(cache_id,
							buffer.sliced(0, server_recv_size));
					}

					qInfo() << QString("%1成功收到%2的响应数据 响应数据长度:%3")
						.arg(should_cache ? "[已存入缓存] " : "")
						.arg(header.host())
						.arg(server_recv_size);

					// 将数据发回客户端
					// 由于可能存在分块发送，所以不应包含结尾'\0'，否则会导致数据中间被插入'\0'
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

		Sleep(20);
	}
}
