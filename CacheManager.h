#pragma once

#include <mutex>
#include <qfile.h>
#include <qbytearray.h>
#include <qbytearraylist.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qjsondocument.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

struct CacheQueryResult
{
	bool is_existent;
	QString last_modified;
	int cache_id;
};

class CacheManager
{
private:
	static const QString kCacheDir;
	static const QString kCacheIndexPath;
	static const QString kCacheNamePattern;

	// 为减少磁盘访问次数，仅在程序启动时从磁盘读入缓存索引，
	// 以后每次查询缓存索引，都在内存中进行。
	// 每次更新缓存索引时，同时更新内存中的索引和磁盘中的索引。
	static QJsonArray index_array_;

	static std::mutex index_array_mutex_;
public:
	static void LoadFromDisk();

	static CacheQueryResult QueryCache(const QString& url);

	static QByteArrayList ReadCacheChunks(const int cache_id);

	// 返回新缓存的cacheId
	static int CreateCache(const QString& url,const QString& last_modified);

	static void AppendCacheChunk(const int cache_id, const QByteArray& data);
};

