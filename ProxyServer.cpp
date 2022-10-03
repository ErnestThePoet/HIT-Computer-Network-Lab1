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

	RuleManager::LoadFromDisk();
	CacheManager::LoadFromDisk();
	this->RunServiceLoop();
}

void ProxyServer::RunServiceLoop() const
{
	const char* kErrorPrefix = "������ʧ�ܡ�: ";

	while (true)
	{
		// ͬʱ��ȡ�ͻ���IP��ַ
		SOCKADDR_IN client_socket_addr;
		int client_addr_size = sizeof(SOCKADDR);

		auto to_client_socket = accept(this->server_socket_, 
			reinterpret_cast<SOCKADDR*>(& client_socket_addr),
			&client_addr_size);

		QString client_ip(inet_ntoa(client_socket_addr.sin_addr));

		std::thread service_thread([kErrorPrefix, to_client_socket,client_ip]
			{
				// 2MB
				constexpr int kBufferSize = 2*1024*1024;

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

				// Ӧ�ù��˹���
				if (RuleManager::isHostForbidden(header.host()))
				{
					qInfo() << QString("[���˹�����Ч] ��ֹ��������%1")
						.arg(header.host());

					auto forbidden_response = ResponseUtil::GetForbiddenResponse();
					send(to_client_socket,
						forbidden_response.constData(),
						forbidden_response.size(),
						0);
					closesocket(to_client_socket);
					return;
				}

				if (RuleManager::isUserForbidden(client_ip))
				{
					qInfo() << QString("[���˹�����Ч] ��ֹ�û�%1���ʻ�����")
						.arg(client_ip);

					auto forbidden_response = ResponseUtil::GetForbiddenResponse();
					send(to_client_socket,
						forbidden_response.constData(),
						forbidden_response.size(),
						0);
					closesocket(to_client_socket);
					return;
				}

				if (RuleManager::shouldHostRedirect(header.host()))
				{
					qInfo() << QString("[���˹�����Ч] ����������%1�ķ���������ҳ��")
						.arg(header.host());

					auto hacked_response = ResponseUtil::GetHackedResponse();
					send(to_client_socket,
						hacked_response.constData(),
						hacked_response.size(),
						0);
					closesocket(to_client_socket);
					return;
				}

				// ������GET/POST/PUT/DELETE����
				if (header.method() == "GET" 
					|| header.method() == "POST"
					|| header.method() == "PUT"
					|| header.method() == "DELETE")
				{
					// ��ӡ������Ϣ
					qInfo() << QString("[%1] %2 %3 ��������:%4 �������ݳ���:%5 ʱ��:%6")
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

				// ��ѯ�������Ƿ��и�url�������Ļ���
				auto cache_query_result = CacheManager::QueryCache(header.url());

				// ���л��棬����������Ͱ���If-Modified-Since�ֶε�����
				// ��ѯ�����Ƿ�Ϊ���£�
				// ���޻��棬ǿ��ɾ���ͻ�������ͷ�е�If-Modified-Since�ֶΣ�
				// ʹ�÷��������᷵��304����ʱ����Ч���ݣ����ܴ��뻺�棩
				if (cache_query_result.is_existent)
				{
					// ���ԭ����ͷ����If-Modified-Since����ô�����滻��
					// �������һ���ֶ�
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

					qInfo() << QString("[��������] %1 ��������ͷ����If-Modified-Since�ֶ�")
						.arg(header.url());

					qInfo() << buffer.sliced(0, 700);
				}
				else
				{
					int modified_since_index = buffer.indexOf("If-Modified-Since: ");

					if (modified_since_index != -1)
					{
						int crlf_index = buffer.indexOf("\r\n", modified_since_index);

						buffer.remove(modified_since_index, 
							crlf_index - modified_since_index + 2);
					}
				}

				// �������������������
				// ʵ��֤�������Ȳ�����Ҫ������β'\0'�����������������ӦĳЩ����
				status = send(to_server_socket,
					buffer.constData(),
					client_recv_size+1,
					0);

				if (status == SOCKET_ERROR)
				{
					STOP_SERVICE_CLOSE2(to_server_socket,
						to_client_socket,
						kErrorPrefix,
						QString("���������������ʱsend()����ʧ�ܣ�����ֵΪ%1")
						.arg(WSAGetLastError()));
				}

				// ���ڻ���δ���е����������Ӧ����Lat-Modified�ֶβ�Ӧ�ý��л���
				bool should_cache = false;
				int cache_id = -1;

				// �������������Ӧ���ݿ飬������ؿͻ���
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
								"���շ�������Ӧ����ʱrecv()����ʧ�ܣ�����ֵΪ%1")
							.arg(WSAGetLastError()));
					}

					// ���֮ǰ������������Ӧ��Ϊ304����ôֱ�ӷֿ鷵�ػ�������
					if (i == 0 
						&& cache_query_result.is_existent
						&& buffer.split('\r')[0].contains("304")
					)
					{
						qInfo() << QString("[304 Not Modified] %1 �����ػ�������")
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
										"��ͻ��˷�������ʱsend()����ʧ�ܣ�����ֵΪ%1")
									.arg(WSAGetLastError()));
							}
						}

						break;
					}

					// �������δ���л���ʧЧ����ô���뻺��
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

					qInfo() << QString("%1�ɹ��յ�%2����Ӧ���� ��Ӧ���ݳ���:%3")
						.arg(should_cache 
							? (cache_query_result.is_existent
								?"[�Ѹ��»���] "
								:"[�Ѵ��뻺��] ") 
							: "")
						.arg(header.host())
						.arg(server_recv_size);

					// �����ݷ��ؿͻ���
					// ���ڿ��ܴ��ڷֿ鷢�ͣ����Բ�Ӧ������β'\0'������ᵼ�������м䱻����'\0'
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

		Sleep(20);
	}
}
