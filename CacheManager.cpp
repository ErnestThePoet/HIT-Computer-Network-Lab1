#include "CacheManager.h"

const QString CacheManager::kCacheDir = "./cache";
const QString CacheManager::kCacheIndexPath = "./cache/index.json";
QJsonArray CacheManager::index_array_ = QJsonArray();

void CacheManager::LoadFromDisk()
{
    QFile index_file(CacheManager::kCacheIndexPath);

    if (!index_file.exists())
    {
        return;
    }
    else
    {
        index_file.open(QIODeviceBase::ReadOnly);
    }

    QByteArray file_content = index_file.readAll();
    index_file.close();

    QJsonDocument index_document = QJsonDocument::fromJson(file_content);
    CacheManager::index_array_ = index_document.array();
}

CacheQueryResult CacheManager::QueryCache(const QString& url)
{
    /*
    File format:
    Array<{
        "url":string;
        "lastModified":string;
        "cacheId":number;
    }>
    */
    for (auto i : CacheManager::index_array_)
    {
        QJsonObject current_entry = i.toObject();
        if (current_entry.value("url") == url)
        {
            return {
                true,
                current_entry.value("lastModified").toString(),
                QString("cache-%1.json")
                .arg(current_entry.value("cacheId").toInt())
            };
        }
    }

    return{ false,"","" };
}

QStringList CacheManager::ReadCacheChunks(const QString& cache_path)
{
    QFile cache_file(cache_path);
    cache_file.open(QIODeviceBase::ReadOnly);

    QByteArray file_content = cache_file.readAll();
    cache_file.close();

    QJsonArray chunk_array = QJsonDocument::fromJson(file_content).array();
    QStringList chunks(chunk_array.size());
    for (int i=0;i<chunk_array.size();i++)
    {
        chunks[i] = chunk_array[i].toString();
    }

    return chunks;
}

void CacheManager::CreateCache(const QString& url, const QString& last_modified)
{
    int max_id = -1;

    if (CacheManager::index_array_.size() > 0)
    {
        max_id= CacheManager::index_array_
            [CacheManager::index_array_.size() - 1]
        .toObject().value("cacheId").toInt();
    }

    CacheManager::index_array_.append(QJsonObject({
        {"url",url},
        {"lastModified",last_modified},
        {"cacheId",max_id + 1} }));

    QFile index_file(CacheManager::kCacheIndexPath);

    // 如果索引文件不存在（冷启动），那么新建一个包含空数组的索引文件
    if (!index_file.exists())
    {
        index_file.open(QIODeviceBase::NewOnly);
    }
    else
    {
        index_file.open(QIODeviceBase::WriteOnly);
    }

    index_file.write(QJsonDocument(CacheManager::index_array_).toJson());

    index_file.close();
}

void CacheManager::AppendCacheChunk(const QString & cache_path, const QString & data)
{
    QFile cache_file(cache_path);
    cache_file.open(QIODeviceBase::ReadWrite);

    QByteArray file_content = cache_file.readAll();

    QJsonArray chunk_array = QJsonDocument::fromJson(file_content).array();
    chunk_array.append(QJsonValue(data));

    cache_file.write(QJsonDocument(chunk_array).toJson());

    cache_file.close();
}
