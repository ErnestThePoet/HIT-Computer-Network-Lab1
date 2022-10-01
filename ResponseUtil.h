#pragma once

#include <qbytearray.h>
#include <qfile.h>

class ResponseUtil
{
public:
	static QByteArray GetEmpty500Response()
	{
		return QByteArray("HTTP/1.1 500 Internal Server Error\r\n\r\n");
	}
};

