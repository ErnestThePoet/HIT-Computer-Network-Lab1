#include "ResponseUtil.h"

QByteArray ResponseUtil::GetForbiddenResponse()
{
	return QByteArray("HTTP/1.1 200 OK\r\n\r\n<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>禁止访问互联网</title></head><body><h1>您被禁止访问互联网。</h1></body></html>");
}

QByteArray ResponseUtil::GetHackedResponse()
{
	return QByteArray("HTTP/1.1 200 OK\r\n\r\n<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>钓鱼网站</title></head><body><h1>访问互联网请v我50</h1></body></html>");
}
