#include "ResponseUtil.h"

QByteArray ResponseUtil::GetForbiddenResponse()
{
	return QByteArray("HTTP/1.1 200 OK\r\n\r\n<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>��ֹ���ʻ�����</title></head><body><h1>������ֹ���ʻ�������</h1></body></html>");
}

QByteArray ResponseUtil::GetHackedResponse()
{
	return QByteArray("HTTP/1.1 200 OK\r\n\r\n<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>������վ</title></head><body><h1>���ʻ�������v��50</h1></body></html>");
}
