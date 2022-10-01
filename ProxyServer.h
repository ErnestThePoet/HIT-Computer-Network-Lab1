#pragma once

#include <qdebug.h>
#include <qstring.h>
#include <qvector.h>
#include <thread>
#include <Windows.h>

#pragma comment(lib,"ws2_32.lib")

#define ERR_EXIT(P,M) do{WSACleanup();qCritical()<<(P)<<(M)<<'\n';std::exit(-1);}while(false)

class ProxyServer
{
private:
	SOCKET server_socket_;

	void RunServiceLoop() const;
public:
	ProxyServer():server_socket_(INVALID_SOCKET) {}

	~ProxyServer()
	{
		WSACleanup();
	}

	ProxyServer(const ProxyServer& _) = delete;
	ProxyServer& operator=(const ProxyServer& _) = delete;

	void Start(u_short port = 11960);
};

