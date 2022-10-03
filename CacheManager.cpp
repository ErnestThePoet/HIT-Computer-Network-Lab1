#include "CacheManager.h"

const QString CacheManager::kCacheDir = "./cache";
const QString CacheManager::kCacheIndexPath = 
    CacheManager::kCacheDir+"/index.json";
const QString CacheManager::kCacheNamePattern = 
    CacheManager::kCacheDir + "/cache-%1-%2";

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
        "chunkIds":number[];
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
    QByteArrayList chunks;

    for (auto i:CacheManager::index_array_)
    {
        auto current_index_entry = i.toObject();
        if (current_index_entry.value("cacheId").toInt() == cache_id)
        {
            auto chunk_ids = current_index_entry.value("chunkIds").toArray();
            
            for (auto j : chunk_ids)
            {
                QFile chunk_file(CacheManager::kCacheNamePattern
                    .arg(cache_id)
                    .arg(j.toInt()));
                chunk_file.open(QIODeviceBase::ReadOnly);
                chunks.append(chunk_file.readAll());
                chunk_file.close();
            }

            break;
        }
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
            current_cache_entry.insert("chunkIds", QJsonArray());
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
            {"cacheId",cache_id},
            {"chunkIds",QJsonArray()}}));
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

    return cache_id;
}

void CacheManager::AppendCacheChunk(const int cache_id, const QByteArray& data)
{
    int current_chunk_id = 0;
    for (int i=0;i<CacheManager::index_array_.size();i++)
    {
        auto current_index_entry = CacheManager::index_array_[i].toObject();
        if (current_index_entry.value("cacheId").toInt() == cache_id)
        {
            auto chunk_ids = current_index_entry.value("chunkIds").toArray();
            if (chunk_ids.size() > 0)
            {
                current_chunk_id = chunk_ids.last().toInt() + 1;
            }
            chunk_ids.append(QJsonValue(current_chunk_id));

            current_index_entry.insert("chunkIds", chunk_ids);
            CacheManager::index_array_[i] = current_index_entry;
            break;
        }
    }

    QFile index_file(CacheManager::kCacheIndexPath);
    index_file.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate);
    index_file.write(QJsonDocument(CacheManager::index_array_).toJson());
    index_file.close();

    QFile chunk_file(CacheManager::kCacheNamePattern
        .arg(cache_id)
        .arg(current_chunk_id));
    if (!chunk_file.exists())
    {
        chunk_file.open(QIODeviceBase::NewOnly);
    }
    else
    {
        chunk_file.open(QIODeviceBase::WriteOnly | QIODeviceBase::Truncate);
    }
    
    chunk_file.write(data);
    chunk_file.close();
}
