#pragma once

#include <qdebug.h>
#include <qdatetime.h>
#include <qstring.h>
#include <qbytearray.h>
#include <qvector.h>
#include <qpair.h>
#include <thread>
#include <Windows.h>

#include "HttpHeader.h"
#include "ResponseUtil.h"

#pragma comment(lib,"ws2_32.lib")
// 仅对本文件生效
#pragma execution_character_set("utf-8")

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

