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

	// ��ѯ������Ӧ��������ַ
	HOSTENT* hostent = gethostbyname(host);
	if (hostent == nullptr)
	{
		return { INVALID_SOCKET,
			QString("gethostbyname()��������NULL, ������Ϊ%1")
			.arg(WSAGetLastError()) };
	}

	IN_ADDR server_addr = *reinterpret_cast<IN_ADDR*>(hostent->h_addr_list[0]);
	to_server_socket_addr.sin_addr.S_un.S_addr = inet_addr(inet_ntoa(server_addr));

	SOCKET to_server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (to_server_socket == INVALID_SOCKET)
	{
		return { INVALID_SOCKET,"���������������׽���ʧ��"};
	}

	if (connect(to_server_socket,
		reinterpret_cast<SOCKADDR*>(&to_server_socket_addr),
		sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		closesocket(to_server_socket);
		return { INVALID_SOCKET,
			QString("connect()�������ô���, ������Ϊ%1")
			.arg(WSAGetLastError()) };
	}

	return { to_server_socket,"" };
}

void ProxyServer::Start(u_short port)
{
	const char* kErrorPrefix = "���׽��ֳ�ʼ������: ";

	int status = 0;

	WSADATA wsaData;
	status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0)
	{
		// ���ڻ�û��һ�ζ�WSAStartup�ĳɹ����ã�WSACleanup��Ӧ�ڴ˱�����
		qCritical() << kErrorPrefix
			<< QString("WSAStartup()����ʧ�ܣ�����ֵΪ%1").arg(status);
		std::exit(-1);
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		ERR_EXIT(kErrorPrefix,
			QString("δԤ�ڵ�Windows Sockets�汾: %1.%2")
				.arg(LOBYTE(wsaData.wVersion))
				.arg(HIBYTE(wsaData.wVersion)));
	}

	// ����HTTP����ʹ�õ���TCPЭ�飬�˴��������ʹ��IPv4Э�飬
	// ���ָ������ΪAF_INET(IPv4)��SOCK_STREAM(TCP)
	this->server_socket_ = socket(AF_INET, SOCK_STREAM, 0);

	if (this->server_socket_ == INVALID_SOCKET)
	{
		ERR_EXIT(kErrorPrefix,"�������ͻ��˵��׽���ʧ��");
	}

	SOCKADDR_IN server_socket_addr;
	server_socket_addr.sin_family = AF_INET;
	server_socket_addr.sin_port = htons(port);
	// ʹ�������ַ��ͨ�����
	server_socket_addr.sin_addr.S_un.S_addr = INADDR_ANY;

	status = bind(this->server_socket_, 
		reinterpret_cast<SOCKADDR*>(&server_socket_addr),
		sizeof(SOCKADDR));
	if (status != 0)
	{
		ERR_EXIT(kErrorPrefix,
			QString("bind()����ʧ�ܣ�����ֵΪ%1").arg(status));
	}

	status = listen(this->server_socket_, SOMAXCONN);
	if (status != 0)
	{
		ERR_EXIT(kErrorPrefix,
			QString("listen()����ʧ�ܣ�����ֵΪ%1.").arg(status));
	}

	// ���ü����¼��������
	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	qInfo()<<QString(
		"����������������ɹ��������˿�: %1")
		.arg(port);

	this->RunServiceLoop();
}

void ProxyServer::RunServiceLoop() const
{
	const char* kErrorPrefix = "������ʧ�ܡ�: ";

	while (true)
	{
		auto to_client_socket = accept(this->server_socket_, nullptr, nullptr);

		std::thread service_thread([kErrorPrefix, to_client_socket]
			{
				// 1MB
				constexpr int kBufferSize = 1*1024*1024;

				QByteArray buffer(kBufferSize, '\0');

				// ���տͻ�����������
				int status = recv(to_client_socket, buffer.data(), kBufferSize, 0);
				
				// ʵ��֤��status��������ݳ��Ȳ�������β'\0'
				int client_recv_size = status;

				if (status == SOCKET_ERROR)
				{
					STOP_SERVICE_CLOSE1(to_client_socket,
						kErrorPrefix,
						QString("���տͻ�����������ʱrecv()����ʧ�ܣ�����ֵΪ%1")
							.arg(WSAGetLastError()));
				}

				// ��������ͷ���󣬰�������ؼ���Ϣ
				HttpHeader header(buffer);

				// ������GET/POST/PUT/DELETE����
				if (header.method() == "GET" 
					|| header.method() == "POST"
					|| header.method() == "PUT"
					|| header.method() == "DELETE")
				{
					// ��ӡ������Ϣ
					qDebug() << QString("[%1] %2 %3 ��������:%4 �������ݳ���:%5 ʱ��:%6")
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

				// ���ӵ��ͻ�������ķ�����
				// http�˿�ʹ��80
				auto connect_result = 
					ConnectToServer(header.host().toUtf8(),80);

				SOCKET to_server_socket = connect_result.first;

				if (to_server_socket == INVALID_SOCKET)
				{
					STOP_SERVICE_CLOSE1(to_client_socket,
						kErrorPrefix,
						connect_result.second);
				}

				// �������������������
					// ͬ����������Ϣ���������β'\0'
				status = send(to_server_socket,
					buffer.constData(),
					client_recv_size,
					0);

				if (status == SOCKET_ERROR)
				{
					STOP_SERVICE_CLOSE2(to_server_socket,
						to_client_socket,
						kErrorPrefix,
						QString("���������������ʱsend()����ʧ�ܣ�����ֵΪ%1")
						.arg(WSAGetLastError()));
				}

				// �������������Ӧ���ݿ飬������ؿͻ���
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
								"���շ�������Ӧ����ʱrecv()����ʧ�ܣ�����ֵΪ%1")
							.arg(WSAGetLastError()));
					}

					qDebug() << QString("�ɹ��յ�%1����Ӧ���� ��Ӧ���ݳ���:%2")
						.arg(header.host())
						.arg(server_recv_size);

					// �����ݷ��ؿͻ���
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
								"��ͻ��˷�������ʱsend()����ʧ�ܣ�����ֵΪ%1")
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
