#include "CacheManager.h"

const QString CacheManager::kCacheDir = "./cache";
const QString CacheManager::kCacheIndexPath = 
    CacheManager::kCacheDir+"/index.json";
const QString CacheManager::kCacheNamePattern = 
    CacheManager::kCacheDir + "/cache-%1.json";

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
                current_entry.value("cacheId").toInt()
            };
        }
    }

    return{ false,"",-1 };
}

QByteArrayList CacheManager::ReadCacheChunks(const int cache_id)
{
    QFile cache_file(CacheManager::kCacheNamePattern.arg(cache_id));
    cache_file.open(QIODeviceBase::ReadOnly);

    QByteArray file_content = cache_file.readAll();
    cache_file.close();

    QJsonArray chunk_array = QJsonDocument::fromJson(file_content).array();
    QByteArrayList chunks(chunk_array.size());
    for (int i=0;i<chunk_array.size();i++)
    {
        chunks[i] = QByteArray::fromBase64(chunk_array[i].toString().toUtf8());
    }

    return chunks;
}

int CacheManager::CreateCache(const QString& url, const QString& last_modified)
{
    int cache_id = 0;
    bool is_entry_existent = false;
    // 若该url的缓存索引记录已存在，那么更新该记录
    for (int i = 0; i < CacheManager::index_array_.size(); i++)
    {
        auto current_cache_entry = CacheManager::index_array_[i].toObject();
        if (current_cache_entry.value("url").toString() == url)
        {
            current_cache_entry.insert("lastModified", last_modified);
            cache_id = current_cache_entry.value("cacheId").toInt();

            CacheManager::index_array_[i] = current_cache_entry;
            is_entry_existent = true;
            break;
        }
    }

    // 否则插入一条记录
    if (!is_entry_existent)
    {
        if (CacheManager::index_array_.size() > 0)
        {
            cache_id = CacheManager::index_array_
                [CacheManager::index_array_.size() - 1]
            .toObject().value("cacheId").toInt() + 1;
        }

        CacheManager::index_array_.append(QJsonObject({
            {"url",url},
            {"lastModified",last_modified},
            {"cacheId",cache_id} }));
    }

    QFile index_file(CacheManager::kCacheIndexPath);

    // 如果索引文件不存在（冷启动），那么新建一个索引文件
    if (!index_file.exists())
    {
        index_file.open(QIODeviceBase::NewOnly);
    }
    else
    {
        index_file.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate);
    }

    index_file.write(QJsonDocument(CacheManager::index_array_).toJson());

    index_file.close();

    // 置缓存文件内容为空数组，如果不存在则新建文件
    QFile cache_file(CacheManager::kCacheNamePattern.arg(cache_id));
    if (!cache_file.exists())
    {
        cache_file.open(QIODeviceBase::NewOnly);
    }
    else
    {
        cache_file.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate);
    }

    cache_file.write("[]");
    cache_file.close();

    return cache_id;
}

void CacheManager::AppendCacheChunk(const int cache_id, const QByteArray& data)
{
    QFile cache_file_read(CacheManager::kCacheNamePattern.arg(cache_id));
    cache_file_read.open(QIODeviceBase::ReadOnly);

    QByteArray file_content = cache_file_read.readAll();

    cache_file_read.close();

    QJsonArray chunk_array = QJsonDocument::fromJson(file_content).array();
    chunk_array.append(QJsonValue(data.toBase64().constData()));

    QFile cache_file_write(CacheManager::kCacheNamePattern.arg(cache_id));
    cache_file_write.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate);
    cache_file_write.write(QJsonDocument(chunk_array).toJson());
    cache_file_write.close();
}
