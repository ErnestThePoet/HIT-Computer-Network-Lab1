#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <format>
#include <thread>
#include <Windows.h>

#pragma comment(lib,"ws2_32.lib")

#define ERR_EXIT(P,M) do{WSACleanup();std::cerr<<(P)<<(M)<<std::endl;std::exit(-1);}while(false)

class ProxyServer
{
private:
	u_short port_;
	SOCKET server_socket_;
	SOCKADDR_IN server_socket_addr_;

	void RunServiceLoop() const;
public:
	ProxyServer(u_short port=11960):
		port_(port),server_socket_(INVALID_SOCKET),server_socket_addr_() {}

	~ProxyServer()
	{
		std::cout << "DTOR\n";
		WSACleanup();
	}

	ProxyServer(const ProxyServer& _) = delete;
	ProxyServer& operator=(const ProxyServer& _) = delete;

	void Start();
};

