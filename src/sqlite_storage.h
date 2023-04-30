#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include <memory>
#include <string_view>

#include <sqlite3.h>

#include "vdf_record.h"

class SQLiteStorage
{
public:
    explicit SQLiteStorage(std::string_view file_path);

    ~SQLiteStorage();

    SQLiteStorage(SQLiteStorage const& rhs) = delete;

    SQLiteStorage& operator=(SQLiteStorage const& rhs) = delete;

    SQLiteStorage(SQLiteStorage&& rhs);

    SQLiteStorage& operator=(SQLiteStorage&& rhs);

    void Save(VDFRecordPack const& pack);

    std::tuple<VDFRecord, bool> QueryRecord(int64_t vdf_id) const;

    std::tuple<VDFRecord, bool> QueryLastRecord() const;

    std::vector<VDFRecord> QueryRecords(uint32_t begin_timestamp, uint32_t end_timestamp) const;

    std::vector<VDFRequest> QueryRequests(int64_t vdf_id) const;

    void BeginTransaction();

    void CommitTransaction();

private:
    sqlite3* sql3_ { nullptr };
};

#endif
