#include "local_sqlite_storage.h"

#include <tinyformat.h>

#include <memory>

#include "sqlite_utils.h"

#include "sqlite_stmt_wrap.h"

#include "block_info.h"

LocalSQLiteStorage::LocalSQLiteStorage(std::string_view file_path)
    : sql3_(file_path)
{
    if (sql3_.GetStatus() == SQLite::Status::Created) {
        // creating tables
        sql3_.ExecuteSQL("create table vdf_record (timestamp, challenge, height)");
        sql3_.ExecuteSQL("create table vdf_requests (challenge, iters, estimated_seconds, group_hash, total_size)");
        sql3_.ExecuteSQL("create table vdf_results (challenge, iters, y, proof, witness_type, duration)");
    }

    // ensure that the table `blocks` is created
    sql3_.ExecuteSQL("create table if not exists blocks (hash primary key, timestamp, challenge, height, filter_bits, block_difficulty, challenge_difficulty, farmer_pk, address, reward, accumulate, vdf_time, vdf_iters, vdf_speed)");
}

void LocalSQLiteStorage::Save(VDFRecordPack const& pack)
{
    AppendRecord(pack.record);

    for (auto const& req : pack.requests) {
        AppendRequest(req);
    }

    for (auto const& res : pack.results) {
        AppendResult(res);
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

void LocalSQLiteStorage::AppendResult(VDFResult const& result)
{
    auto stmt = sql3_.Prepare("insert into vdf_results (challenge, iters, y, proof, witness_type, duration) values (?, ?, ?, ?, ?, ?)");
    stmt.Bind(1, result.challenge);
    stmt.Bind(2, result.iters);
    stmt.Bind(3, result.y);
    stmt.Bind(4, result.proof);
    stmt.Bind(5, result.witness_type);
    stmt.Bind(6, result.duration);
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

std::vector<VDFResult> LocalSQLiteStorage::QueryResults(uint256 const& challenge)
{
    std::vector<VDFResult> results;
    auto stmt = sql3_.Prepare("select iters, y, proof, witness_type, duration from vdf_results where challenge = ?");
    stmt.Bind(1, challenge);
    while (stmt.StepNext()) {
        VDFResult result;
        result.challenge = challenge;
        result.iters = stmt.GetColumnInt64(0);
        result.y = stmt.GetColumnBytes(1);
        result.proof = stmt.GetColumnBytes(2);
        result.witness_type = stmt.GetColumnInt64(3);
        result.duration = stmt.GetColumnInt64(4);
        results.push_back(std::move(result));
    }
    return results;
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

int LocalSQLiteStorage::QueryNumHeightsByTimeRange(int hours)
{
    int begin_height, end_height;
    uint32_t end_timestamp;
    {
        auto stmt = sql3_.Prepare("select height, timestamp from vdf_record order by timestamp desc limit 1");
        if (!stmt.StepNext()) {
            return -1;
        }
        end_height = stmt.GetColumnInt64(0);
        end_timestamp = stmt.GetColumnInt64(1);
    }
    uint32_t begin_timestamp = end_timestamp - hours * 60 * 60;
    {
        auto stmt2 = sql3_.Prepare("select height from vdf_record where timestamp <= ? order by timestamp desc limit 1");
        stmt2.Bind(1, begin_timestamp);
        if (!stmt2.StepNext()) {
            return -1;
        }
        begin_height = stmt2.GetColumnInt64(0);
    }
    return end_height - begin_height + 1;
}
