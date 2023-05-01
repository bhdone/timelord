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
        sql3_.ExecuteSQL("create table vdf_record (vdf_id int autoincrement primary key, timestamp, challenge, height, calculated)");
        sql3_.ExecuteSQL("create table vdf_requests (vdf_id, iters, estimated_seconds, group_hash, netspace)");
    }
}

void LocalSQLiteStorage::Save(VDFRecordPack const& pack)
{
    int64_t vdf_id { 0 };
    {
        auto stmt = sql3_.Prepare("insert into vdf_record (timestamp, challenge, height, calculated) values (?, ?, ?, ?)");
        stmt.Bind(1, pack.record.timestamp);
        stmt.Bind(2, pack.record.challenge);
        stmt.Bind(3, pack.record.height);
        stmt.Bind(4, pack.record.calculated);
        stmt.Run();
        vdf_id = stmt.GetLastInsertedRowID();
    }
    assert(vdf_id != 0);

    for (auto const& req : pack.requests) {
        auto stmt = sql3_.Prepare("insert into vdf_requests (vdf_id, iters, estimated_seconds, group_hash, netspace) values (?, ?, ?, ?, ?)");
        stmt.Bind(1, vdf_id);
        stmt.Bind(2, req.iters);
        stmt.Bind(3, req.estimated_seconds);
        stmt.Bind(4, req.group_hash);
        stmt.Bind(5, req.netspace);
        stmt.Run();
    }
}

std::tuple<VDFRecord, bool> LocalSQLiteStorage::QueryRecord(int64_t vdf_id)
{
    VDFRecord record;
    auto stmt = sql3_.Prepare("select vdf_id, timestamp, challenge, height, calculated from vdf_record where vdf_id = ?");
    stmt.Bind(1, vdf_id);
    if (!stmt.StepNext()) {
        return std::make_tuple(record, false);
    }
    record.id = vdf_id;
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
    record.id = stmt.GetColumnInt64(0);
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
        record.id = stmt.GetColumnInt64(0);
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
    auto stmt = sql3_.Prepare("select vdf_id, iters, estimated_seconds, group_hash, netspace from vdf_requests where vdf_id = ?");
    stmt.Bind(1, vdf_id);
    while (stmt.StepNext()) {
        VDFRequest req;
        req.vdf_id = vdf_id;
        req.iters = stmt.GetColumnInt64(1);
        req.estimated_seconds = stmt.GetColumnInt64(2);
        req.group_hash = stmt.GetColumnUint256(3);
        req.netspace = stmt.GetColumnInt64(4);
        requests.push_back(std::move(req));
    }
    return requests;
}
