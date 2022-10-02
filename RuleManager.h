#pragma once

#include <qfile.h>
#include <qbytearray.h>
#include <qbytearraylist.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qjsondocument.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

class RuleManager
{
private:
	static const QString kRulesFilePath;
	static QStringList forbidden_hosts;
	static QStringList forbidden_users;
	static QStringList redirect_hosts;

public:
	static void LoadFromDisk();

	static bool isHostForbidden(const QString& host);
	static bool isUserForbidden(const QString& user);
	static bool shouldHostRedirect(const QString& host);
};

