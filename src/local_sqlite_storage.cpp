#include "local_sqlite_storage.h"

#include <tinyformat.h>

#include <memory>

#include "sqlite_utils.h"

#include "sqlite_stmt_wrap.h"

LocalSQLiteStorage::LocalSQLiteStorage(std::string_view file_path)
    : sql3_(file_path)
{
    if (sql3_.GetStatus() == SQLite::Status::Created) {
        // creating tables
        sql3_.ExecuteSQL("create table vdf_record (vdf_id integer primary key, timestamp, challenge, height, calculated)");
        sql3_.ExecuteSQL("create table vdf_requests (vdf_id, iters, estimated_seconds, group_hash, total_size)");
        sql3_.ExecuteSQL("create table vdf_results (challenge, iters, y, proof, witness_type, duration)");
    }
}

int64_t LocalSQLiteStorage::Save(VDFRecordPack const& pack)
{
    int64_t vdf_id = AppendRecord(pack.record);
    assert(vdf_id != 0);

    for (auto const& req : pack.requests) {
        AppendRequest(vdf_id, req);
    }

    for (auto const& res : pack.results) {
        AppendResult(res);
    }

    return vdf_id;
}

int64_t LocalSQLiteStorage::AppendRecord(VDFRecord const& record)
{
    auto stmt = sql3_.Prepare("insert into vdf_record (timestamp, challenge, height, calculated) values (?, ?, ?, ?)");
    stmt.Bind(1, record.timestamp);
    stmt.Bind(2, record.challenge);
    stmt.Bind(3, record.height);
    stmt.Bind(4, record.calculated);
    stmt.Run();
    return stmt.GetLastInsertedRowID();
}

void LocalSQLiteStorage::AppendRequest(int64_t vdf_id, VDFRequest const& request)
{
    auto stmt = sql3_.Prepare("insert into vdf_requests (vdf_id, iters, estimated_seconds, group_hash, total_size) values (?, ?, ?, ?, ?)");
    stmt.Bind(1, vdf_id);
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

void LocalSQLiteStorage::UpdateRecordCalculated(int64_t vdf_id, bool calculated)
{
    auto stmt = sql3_.Prepare("update vdf_record set calculated = ? where vdf_id = ?");
    stmt.Bind(1, calculated);
    stmt.Bind(2, vdf_id);
    stmt.Run();
}

std::tuple<VDFRecord, bool> LocalSQLiteStorage::QueryRecord(int64_t vdf_id)
{
    VDFRecord record;
    auto stmt = sql3_.Prepare("select vdf_id, timestamp, challenge, height, calculated from vdf_record where vdf_id = ?");
    stmt.Bind(1, vdf_id);
    if (!stmt.StepNext()) {
        return std::make_tuple(record, false);
    }
    record.vdf_id = vdf_id;
    record.timestamp = stmt.GetColumnInt64(1);
    record.challenge = stmt.GetColumnUint256(2);
    record.height = stmt.GetColumnInt64(3);
    record.calculated = stmt.GetColumnInt64(4);
    return std::make_tuple(record, true);
}

std::tuple<VDFRecord, bool> LocalSQLiteStorage::QueryLastRecord()
{
    VDFRecord record;
    auto stmt = sql3_.Prepare("select vdf_id, timestamp, challenge, height, calculated from vdf_record order by vdf_id desc limit 1");
    if (!stmt.StepNext()) {
        return std::make_tuple(record, false);
    }
    record.vdf_id = stmt.GetColumnInt64(0);
    record.timestamp = stmt.GetColumnInt64(1);
    record.challenge = stmt.GetColumnUint256(2);
    record.height = stmt.GetColumnInt64(3);
    record.calculated = stmt.GetColumnInt64(4);
    return std::make_tuple(record, true);
}

std::vector<VDFRecord> LocalSQLiteStorage::QueryRecords(uint32_t begin_timestamp, uint32_t end_timestamp)
{
    std::vector<VDFRecord> records;
    auto stmt = sql3_.Prepare("select vdf_id, timestamp, challenge, height, calculated from vdf_record where timestamp >= ? and timestamp <= ?");
    stmt.Bind(1, begin_timestamp);
    stmt.Bind(2, end_timestamp);
    while (stmt.StepNext()) {
        VDFRecord record;
        record.vdf_id = stmt.GetColumnInt64(0);
        record.timestamp = stmt.GetColumnInt64(1);
        record.challenge = stmt.GetColumnUint256(2);
        record.height = stmt.GetColumnInt64(3);
        record.calculated = stmt.GetColumnInt64(4);
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<VDFRequest> LocalSQLiteStorage::QueryRequests(int64_t vdf_id)
{
    std::vector<VDFRequest> requests;
    auto stmt = sql3_.Prepare("select vdf_id, iters, estimated_seconds, group_hash, total_size from vdf_requests where vdf_id = ?");
    stmt.Bind(1, vdf_id);
    while (stmt.StepNext()) {
        VDFRequest req;
        req.vdf_id = vdf_id;
        req.iters = stmt.GetColumnInt64(1);
        req.estimated_seconds = stmt.GetColumnInt64(2);
        req.group_hash = stmt.GetColumnUint256(3);
        req.total_size = stmt.GetColumnInt64(4);
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
