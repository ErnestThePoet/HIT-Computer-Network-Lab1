#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <format>
#include <Windows.h>

#define ERR_EXIT(M) do{std::cerr<<(M)<<std::endl;std::exit(-1);}while(false)

class ProxyServer
{
private:
	u_short port_ = 0;

	void Log(const std::string& message)
	{
		std::cout << message << std::endl;
	}
public:
	ProxyServer(u_short port=11960):port_(port){}

	void Start();
};

