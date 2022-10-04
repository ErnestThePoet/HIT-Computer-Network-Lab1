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

	// Ϊ���ٴ��̷��ʴ��������ڳ�������ʱ�Ӵ��̶��뻺��������
	// �Ժ�ÿ�β�ѯ���������������ڴ��н��С�
	// ÿ�θ��»�������ʱ��ͬʱ�����ڴ��е������ʹ����е�������
	static QJsonArray index_array_;

	static std::mutex index_array_mutex_;
public:
	static void LoadFromDisk();

	static CacheQueryResult QueryCache(const QString& url);

	static QByteArrayList ReadCacheChunks(const int cache_id);

	// �����»����cacheId
	static int CreateCache(const QString& url,const QString& last_modified);

	static void AppendCacheChunk(const int cache_id, const QByteArray& data);
};

