#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include <string_view>

#include "sqlite_wrap.h"

#include "vdf_record.h"

class LocalSQLiteStorage
{
public:
    explicit LocalSQLiteStorage(std::string_view file_path);

    void Save(VDFRecordPack const& pack);

    std::tuple<VDFRecord, bool> QueryRecord(int64_t vdf_id);

    std::tuple<VDFRecord, bool> QueryLastRecord();

    std::vector<VDFRecord> QueryRecords(uint32_t begin_timestamp, uint32_t end_timestamp);

    std::vector<VDFRequest> QueryRequests(int64_t vdf_id);

private:
    SQLite sql3_;
};

#endif
