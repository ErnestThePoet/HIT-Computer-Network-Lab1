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

	// Ϊ���ٴ��̷��ʴ��������ڳ�������ʱ�Ӵ��̶��뻺��������
	// �Ժ�ÿ�β�ѯ���������������ڴ��н��С�
	// ÿ�θ��»�������ʱ��ͬʱ�����ڴ��е������ʹ����е�������
	static QJsonArray index_array_;
public:
	static void LoadFromDisk();

	static CacheQueryResult QueryCache(const QString& url);

	static QStringList ReadCacheChunks(const QString& cache_path);

	static void CreateCache(const QString& url,const QString& last_modified);

	static void AppendCacheChunk(const QString& cache_path, const QString& data);
};

