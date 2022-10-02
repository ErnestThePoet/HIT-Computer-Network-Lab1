#pragma once

#include <qstring.h>
#include <qstringlist.h>
#include <qdir.h>
#include <qjsondocument.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

struct CacheQueryResult
{
	bool is_existent;
	QString last_modified;
	QString cache_path;
};

class CacheManager
{
private:
	static const QString kCacheDir;
	static const QString kCacheIndexPath;
public:
	static CacheQueryResult QueryCache(const QString& url)
	{
		
	}
};

