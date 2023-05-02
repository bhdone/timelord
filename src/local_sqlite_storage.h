#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include <string_view>

#include "sqlite_wrap.h"

#include "vdf_record.h"

class LocalSQLiteStorage
{
public:
    explicit LocalSQLiteStorage(std::string_view file_path);

    int64_t Save(VDFRecordPack const& pack);

    int64_t AppendRecord(VDFRecord const& record);

    void AppendRequest(int64_t vdf_id, VDFRequest const& request);

    void AppendResult(VDFResult const& result);

    std::tuple<VDFRecord, bool> QueryRecord(int64_t vdf_id);

    std::tuple<VDFRecord, bool> QueryLastRecord();

    std::vector<VDFRecord> QueryRecords(uint32_t begin_timestamp, uint32_t end_timestamp);

    std::vector<VDFRequest> QueryRequests(int64_t vdf_id);

    std::vector<VDFResult> QueryResults(uint256 const& challenge);

private:
    SQLite sql3_;
};

#endif
