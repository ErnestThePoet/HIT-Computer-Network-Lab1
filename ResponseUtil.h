#pragma once

#include <qbytearray.h>
#include <qfile.h>

#include "setencoding.h"

class ResponseUtil
{
public:
	static QByteArray GetForbiddenResponse();
	static QByteArray GetHackedResponse();
};

