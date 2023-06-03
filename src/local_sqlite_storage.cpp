#include "local_sqlite_storage.h"

#include <tinyformat.h>

#include <memory>

#include "sqlite_utils.h"

#include "sqlite_stmt_wrap.h"

#include "block_info.h"

LocalSQLiteStorage::LocalSQLiteStorage(std::string_view file_path)
    : sql3_(file_path)
{
    sql3_.ExecuteSQL("create table if not exists vdf_record (timestamp, challenge, height)");
    sql3_.ExecuteSQL("create index if not exists vdf_record_challenge_idx on vdf_record (challenge)");
    sql3_.ExecuteSQL("create index if not exists vdf_record_timestamp_idx on vdf_record (timestamp)");

    sql3_.ExecuteSQL("create table if not exists vdf_requests (challenge, iters, estimated_seconds, group_hash, total_size)");
    sql3_.ExecuteSQL("create index if not exists vdf_requests_challenge on vdf_requests (challenge)");
    sql3_.ExecuteSQL("create unique index if not exists vdf_requests_challenge_group_hash on vdf_requests (challenge, group_hash)");

    sql3_.ExecuteSQL("create table if not exists blocks (hash primary key, timestamp, challenge, height, filter_bits, block_difficulty, challenge_difficulty, farmer_pk, address, reward, accumulate, vdf_time, vdf_iters, vdf_speed)");
    sql3_.ExecuteSQL("create index if not exists blocks_challenge on blocks (challenge)");
    sql3_.ExecuteSQL("create index if not exists blocks_height on blocks (height)");
    sql3_.ExecuteSQL("create index if not exists blocks_address on blocks (address)");
}

void LocalSQLiteStorage::Save(VDFRecordPack const& pack)
{
    AppendRecord(pack.record);

    for (auto const& req : pack.requests) {
        AppendRequest(req);
    }
}

void LocalSQLiteStorage::AppendRecord(VDFRecord const& record)
{
    auto stmt = sql3_.Prepare("insert into vdf_record (timestamp, challenge, height) values (?, ?, ?)");
    stmt.Bind(1, record.timestamp);
    stmt.Bind(2, record.challenge);
    stmt.Bind(3, record.height);
    stmt.Run();
}

void LocalSQLiteStorage::AppendRequest(VDFRequest const& request)
{
    auto stmt = sql3_.Prepare("insert into vdf_requests (challenge, iters, estimated_seconds, group_hash, total_size) values (?, ?, ?, ?, ?)");
    stmt.Bind(1, request.challenge);
    stmt.Bind(2, request.iters);
    stmt.Bind(3, request.estimated_seconds);
    stmt.Bind(4, request.group_hash);
    stmt.Bind(5, request.total_size);
    stmt.Run();
}

void LocalSQLiteStorage::AppendBlock(BlockInfo const& block_info)
{
    auto stmt = sql3_.Prepare("insert into blocks (hash, timestamp, challenge, height, filter_bits, block_difficulty, challenge_difficulty, farmer_pk, address, reward, accumulate, vdf_time, vdf_iters, vdf_speed) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    stmt.Bind(1, block_info.hash);
    stmt.Bind(2, block_info.timestamp);
    stmt.Bind(3, block_info.challenge);
    stmt.Bind(4, block_info.height);
    stmt.Bind(5, block_info.filter_bits);
    stmt.Bind(6, block_info.block_difficulty);
    stmt.Bind(7, block_info.challenge_difficulty);
    stmt.Bind(8, block_info.farmer_pk);
    stmt.Bind(9, block_info.address);
    stmt.Bind(10, block_info.reward);
    stmt.Bind(11, block_info.accumulate);
    stmt.Bind(12, block_info.vdf_time);
    stmt.Bind(13, block_info.vdf_iters);
    stmt.Bind(14, block_info.vdf_speed);
    stmt.Run();
}

std::tuple<VDFRecord, bool> LocalSQLiteStorage::QueryRecord(uint256 const& challenge)
{
    VDFRecord record;
    auto stmt = sql3_.Prepare("select timestamp, challenge, height from vdf_record where challenge = ?");
    stmt.Bind(1, challenge);
    if (!stmt.StepNext()) {
        return std::make_tuple(record, false);
    }
    record.timestamp = stmt.GetColumnInt64(0);
    record.challenge = stmt.GetColumnUint256(1);
    record.height = stmt.GetColumnInt64(2);
    return std::make_tuple(record, true);
}

std::vector<VDFRecord> LocalSQLiteStorage::QueryRecords(uint32_t begin_timestamp, uint32_t end_timestamp)
{
    std::vector<VDFRecord> records;
    auto stmt = sql3_.Prepare("select timestamp, challenge, height from vdf_record where timestamp >= ? and timestamp <= ?");
    stmt.Bind(1, begin_timestamp);
    stmt.Bind(2, end_timestamp);
    while (stmt.StepNext()) {
        VDFRecord record;
        record.timestamp = stmt.GetColumnInt64(0);
        record.challenge = stmt.GetColumnUint256(1);
        record.height = stmt.GetColumnInt64(2);
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<VDFRequest> LocalSQLiteStorage::QueryRequests(uint256 const& challenge)
{
    std::vector<VDFRequest> requests;
    auto stmt = sql3_.Prepare("select iters, estimated_seconds, group_hash, total_size from vdf_requests where challenge = ?");
    stmt.Bind(1, challenge);
    while (stmt.StepNext()) {
        VDFRequest req;
        req.challenge = challenge;
        req.iters = stmt.GetColumnInt64(0);
        req.estimated_seconds = stmt.GetColumnInt64(1);
        req.group_hash = stmt.GetColumnUint256(2);
        req.total_size = stmt.GetColumnInt64(3);
        requests.push_back(std::move(req));
    }
    return requests;
}

std::vector<BlockInfo> LocalSQLiteStorage::QueryBlocksRange(int num_heights)
{
    std::vector<BlockInfo> blocks;
    auto stmt = sql3_.Prepare("select hash, timestamp, challenge, height, filter_bits, block_difficulty, challenge_difficulty, farmer_pk, address, reward, accumulate, vdf_time, vdf_iters, vdf_speed from blocks order by height desc limit ?");
    stmt.Bind(1, num_heights);
    while (stmt.StepNext()) {
        BlockInfo block_info;
        block_info.hash = stmt.GetColumnUint256(0);
        block_info.timestamp = stmt.GetColumnInt64(1);
        block_info.challenge = stmt.GetColumnUint256(2);
        block_info.height = stmt.GetColumnInt64(3);
        block_info.filter_bits = stmt.GetColumnInt64(4);
        block_info.block_difficulty = stmt.GetColumnInt64(5);
        block_info.challenge_difficulty = stmt.GetColumnInt64(6);
        block_info.farmer_pk = stmt.GetColumnBytes(7);
        block_info.address = stmt.GetColumnString(8);
        block_info.reward = stmt.GetColumnReal(9);
        block_info.accumulate = stmt.GetColumnReal(10);
        block_info.vdf_time = stmt.GetColumnString(11);
        block_info.vdf_iters = stmt.GetColumnInt64(12);
        block_info.vdf_speed = stmt.GetColumnInt64(13);
        blocks.push_back(std::move(block_info));
    }
    return blocks;
}

int LocalSQLiteStorage::QueryLastBlockHeight()
{
    auto stmt = sql3_.Prepare("select height from blocks order by height desc limit 1");
    if (stmt.StepNext()) {
        return stmt.GetColumnInt64(0);
    }
    return -1; // cannot find any record from table `block`
}

int LocalSQLiteStorage::QueryNumHeightsByTimeRange(int pass_hours, int min_height)
{
    int begin_height, end_height;
    uint32_t end_timestamp;
    {
        auto stmt = sql3_.Prepare("select height, timestamp from vdf_record where height >= ? order by timestamp desc limit 1");
        stmt.Bind(1, min_height);
        if (!stmt.StepNext()) {
            return -1;
        }
        end_height = stmt.GetColumnInt64(0);
        end_timestamp = stmt.GetColumnInt64(1);
    }
    uint32_t begin_timestamp = end_timestamp - pass_hours * 60 * 60;
    {
        auto stmt2 = sql3_.Prepare("select height from vdf_record where timestamp <= ? and height >= ? order by timestamp desc limit 1");
        stmt2.Bind(1, begin_timestamp);
        stmt2.Bind(2, min_height);
        if (!stmt2.StepNext()) {
            return -1;
        }
        begin_height = stmt2.GetColumnInt64(0);
    }
    return end_height - begin_height + 1;
}

std::vector<NetspaceData> LocalSQLiteStorage::QueryNetspace(int num_heights, bool sum_netspace)
{
    std::vector<NetspaceData> results;
    char const* SZ_QUERY_NETSPACE_SUM_NETSPACE = "select blocks.height, blocks.challenge_difficulty, blocks.block_difficulty, sum(vdf_requests.total_size) from blocks left join vdf_requests on vdf_requests.challenge = blocks.challenge group by blocks.challenge order by blocks.height desc limit ?";
    char const* SZ_QUERY_NETSPACE = "select height, challenge_difficulty, block_difficulty, 0 from blocks order by height desc limit ?";
    std::string sql_str = sum_netspace ? SZ_QUERY_NETSPACE_SUM_NETSPACE : SZ_QUERY_NETSPACE;
    auto stmt = sql3_.Prepare(sql_str);
    stmt.Bind(1, num_heights);
    while (stmt.StepNext()) {
        NetspaceData data;
        data.height = stmt.GetColumnInt64(0);
        data.challenge_difficulty = stmt.GetColumnInt64(1);
        data.block_difficulty = stmt.GetColumnInt64(2);
        data.netspace = stmt.GetColumnInt64(3);
        results.push_back(std::move(data));
    }
    return results;
}

std::vector<RankRecord> LocalSQLiteStorage::QueryRank(int from_height, int count)
{
    auto stmt = sql3_.Prepare("select farmer_pk, sum(reward), count(*), avg(block_difficulty) from blocks where height >= ? group by farmer_pk limit ?");
    stmt.Bind(1, from_height);
    stmt.Bind(2, count);
    std::vector<RankRecord> res;
    while (stmt.StepNext()) {
        RankRecord rank;
        rank.farmer_pk = stmt.GetColumnString(0);
        rank.total_reward = stmt.GetColumnInt64(1);
        rank.produced_blocks = stmt.GetColumnInt64(2);
        rank.participated_blocks = 0;
        rank.average_difficulty = stmt.GetColumnInt64(3);
        res.push_back(std::move(rank));
    }
    return res;
}

uint64_t LocalSQLiteStorage::QueryMaxNetspace(int from_height, int best_height)
{
    char const* SZ_QUERY_NETSPACE_MAX_SUM_NETSPACE = "select sum(vdf_requests.total_size) as s from blocks left join vdf_requests on vdf_requests.challenge = blocks.challenge where blocks.height >= ? and blocks.height < ? group by blocks.challenge order by s desc limit 1";
    auto stmt = sql3_.Prepare(SZ_QUERY_NETSPACE_MAX_SUM_NETSPACE);
    stmt.Bind(1, from_height);
    stmt.Bind(2, best_height);
    if (stmt.StepNext()) {
        return stmt.GetColumnInt64(0);
    }
    return 0;
}

uint64_t LocalSQLiteStorage::QueryMinNetspace(int from_height, int best_height)
{
    char const* SZ_QUERY_NETSPACE_MIN_SUM_NETSPACE = "select sum(vdf_requests.total_size) as s from blocks left join vdf_requests on vdf_requests.challenge = blocks.challenge where blocks.height >= ? and blocks.height < ? group by blocks.challenge order by s limit 1";
    auto stmt = sql3_.Prepare(SZ_QUERY_NETSPACE_MIN_SUM_NETSPACE);
    stmt.Bind(1, from_height);
    stmt.Bind(2, best_height);
    if (stmt.StepNext()) {
        return stmt.GetColumnInt64(0);
    }
    return 0;
}
