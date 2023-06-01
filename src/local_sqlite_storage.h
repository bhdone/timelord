#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include <string_view>

#include "sqlite_wrap.h"

#include "vdf_record.h"

#include "netspace_data.h"

#include "rank_record.h"

#include "local_database_keeper.hpp"

struct BlockInfo;

class LocalSQLiteStorage
{
public:
    explicit LocalSQLiteStorage(std::string_view file_path);

    void Save(VDFRecordPack const& pack);

    void AppendRecord(VDFRecord const& record);

    void AppendRequest(VDFRequest const& request);

    void AppendBlock(BlockInfo const& block_info);

    std::tuple<VDFRecord, bool> QueryRecord(uint256 const& challenge);

    std::vector<VDFRecord> QueryRecords(uint32_t begin_timestamp, uint32_t end_timestamp);

    std::vector<VDFRequest> QueryRequests(uint256 const& challenge);

    std::vector<BlockInfo> QueryBlocksRange(int num_heights);

    int QueryLastBlockHeight();

    int QueryNumHeightsByTimeRange(int pass_hours, int min_height);

    std::vector<NetspaceData> QueryNetspace(int num_heights, bool sum_netspace);

    std::vector<RankRecord> QueryRank(int from_height, int count);

    uint64_t QueryMaxNetspace(int from_height, int best_height);

    uint64_t QueryMinNetspace(int from_height, int best_height);

private:
    SQLite sql3_;
};

using LocalSQLiteDatabaseKeeper = LocalDatabaseKeeper<LocalSQLiteStorage>;

#endif
