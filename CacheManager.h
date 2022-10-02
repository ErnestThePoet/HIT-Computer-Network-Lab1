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

	// 为减少磁盘访问次数，仅在程序启动时从磁盘读入缓存索引，
	// 以后每次查询缓存索引，都在内存中进行。
	// 每次更新缓存索引时，同时更新内存中的索引和磁盘中的索引。
	static QJsonArray index_array_;
public:
	static void LoadFromDisk();

	static CacheQueryResult QueryCache(const QString& url);

	static QStringList ReadCacheChunks(const QString& cache_path);

	static void CreateCache(const QString& url,const QString& last_modified);

	static void AppendCacheChunk(const QString& cache_path, const QString& data);
};

