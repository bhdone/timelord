#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include <string_view>

#include "sqlite_wrap.h"

#include "vdf_record.h"

#include "vdf_persist_data.h"

class LocalSQLiteStorage
{
public:
    explicit LocalSQLiteStorage(std::string_view file_path);

    void Save(VDFRecordPack const& pack);

    void AppendRecord(VDFRecord const& record);

    void AppendRequest(VDFRequest const& request);

    void AppendResult(VDFResult const& result);

    std::tuple<VDFRecord, bool> QueryLastRecord();

    std::vector<VDFRecord> QueryRecords(uint32_t begin_timestamp, uint32_t end_timestamp);

    std::vector<VDFRequest> QueryRequests(uint256 const& challenge);

    std::vector<VDFResult> QueryResults(uint256 const& challenge);

private:
    SQLite sql3_;
};

using VDFSQLitePersistOperator = VDFPersistOperator<LocalSQLiteStorage>;

#endif
