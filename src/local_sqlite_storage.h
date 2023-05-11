#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include <string_view>

#include "sqlite_wrap.h"

#include "vdf_record.h"

#include "local_database_keeper.hpp"

struct BlockInfo;

class LocalSQLiteStorage
{
public:
    explicit LocalSQLiteStorage(std::string_view file_path);

    void Save(VDFRecordPack const& pack);

    void AppendRecord(VDFRecord const& record);

    void AppendRequest(VDFRequest const& request);

    void AppendResult(VDFResult const& result);

    void AppendBlock(BlockInfo const& block_info);

    std::tuple<VDFRecord, bool> QueryRecord(uint256 const& challenge);

    std::vector<VDFRecord> QueryRecords(uint32_t begin_timestamp, uint32_t end_timestamp);

    std::vector<VDFRequest> QueryRequests(uint256 const& challenge);

    std::vector<VDFResult> QueryResults(uint256 const& challenge);

    std::vector<BlockInfo> QueryBlocksRange(int num_heights);

    int QueryNumHeightsByTimeRange(int hours);

private:
    SQLite sql3_;
};

using LocalSQLiteDatabaseKeeper = LocalDatabaseKeeper<LocalSQLiteStorage>;

#endif
