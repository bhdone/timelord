#include "sqlite_storage.h"

#include <tinyformat.h>

#include <memory>

#include <filesystem>
namespace fs = std::filesystem;

#include "timelord_utils.h"

void ExecuteSQL(sqlite3* sql3, std::string_view sql)
{
    int res = sqlite3_exec(sql3, sql.data(), nullptr, nullptr, nullptr);
    if (res != SQLITE_OK) {
        throw std::runtime_error(tinyformat::format("cannot execute sql: %s", sql));
    }
}

void CheckSQL(sqlite3* sql3, int res)
{
    if (res != SQLITE_OK) {
        // get error string
        char const* errmsg = sqlite3_errmsg(sql3);
        throw std::runtime_error(tinyformat::format("SQLite error: %s", errmsg));
    }
}

class StmtWrap
{
public:
    StmtWrap(sqlite3* sql3, std::string_view sql)
        : sql3_(sql3)
    {
        CheckSQL(sql3, sqlite3_prepare_v2(sql3_, sql.data(), -1, &stmt_, nullptr));
    }

    ~StmtWrap()
    {
        try {
            Close();
        } catch (std::exception const&) {
            // ignore exception
        }
    }

    void Close()
    {
        if (stmt_) {
            CheckSQL(sql3_, sqlite3_finalize(stmt_));
        }
    }

    StmtWrap(StmtWrap const& rhs) = delete;

    StmtWrap& operator=(StmtWrap const& rhs) = delete;

    StmtWrap(StmtWrap&& rhs)
    {
        stmt_ = rhs.stmt_;
        rhs.stmt_ = nullptr;
    }

    StmtWrap& operator=(StmtWrap&& rhs)
    {
        if (&rhs != this) {
            stmt_ = rhs.stmt_;
            rhs.stmt_ = nullptr;
        }
        return *this;
    }

    void Bind(int index, std::string_view str)
    {
        CheckSQL(sql3_, sqlite3_bind_text(stmt_, index, str.data(), -1, nullptr));
    }

    void Bind(int index, int64_t val)
    {
        CheckSQL(sql3_, sqlite3_bind_int64(stmt_, index, val));
    }

    void Bind(int index, uint256 const& val)
    {
        Bind(index, Uint256ToHex(val));
    }

    void Run()
    {
        int res = sqlite3_step(stmt_);
        if (res != SQLITE_DONE) {
            char const* errmsg = sqlite3_errmsg(sql3_);
            throw std::runtime_error(tinyformat::format("failed to run sql, error: %s", errmsg));
        }
    }

    bool StepNext()
    {
        int res = sqlite3_step(stmt_);
        if (res == SQLITE_ROW) {
            return true;
        } else if (res == SQLITE_DONE) {
            return false;
        }
        char const* errmsg = sqlite3_errmsg(sql3_);
        throw std::runtime_error(tinyformat::format("cannot retrieve next row from sql database, error: %s", errmsg));
    }

    int64_t GetLastInsertedRowID() const
    {
        return sqlite3_last_insert_rowid(sql3_);
    }

    int64_t GetColumnInt64(int index) const
    {
        return sqlite3_column_int64(stmt_, index);
    }

    std::string GetColumnString(int index) const
    {
        return reinterpret_cast<char const*>(sqlite3_column_text(stmt_, index));
    }

    uint256 GetColumnUint256(int index) const
    {
        return Uint256FromHex(GetColumnString(index));
    }

private:
    sqlite3* sql3_;
    sqlite3_stmt* stmt_ { nullptr };
};

SQLiteStorage::SQLiteStorage(std::string_view file_path)
{
    if (fs::exists(file_path) && fs::is_regular_file(file_path)) {
        if (sqlite3_open(file_path.data(), &sql3_) != SQLITE_OK) {
            throw std::runtime_error(tinyformat::format("cannot open existing sqlite database %s", file_path));
        }
    } else {
        if (sqlite3_open(file_path.data(), &sql3_) != SQLITE_OK) {
            throw std::runtime_error(tinyformat::format("cannot create a new sqlite database %s", file_path));
        }
        // creating tables
        ExecuteSQL(sql3_,
                "create table vdf_record (vdf_id int autoincrement primary key, timestamp, challenge, calculated)");
        ExecuteSQL(sql3_, "create table vdf_requests (vdf_id, iters, estimated_seconds, group_hash, netspace)");
    }
}

SQLiteStorage::~SQLiteStorage()
{
    if (sql3_) {
        int res = sqlite3_close(sql3_);
        if (res != SQLITE_OK) {
            // TODO: handle error here
        }
    }
}

SQLiteStorage::SQLiteStorage(SQLiteStorage&& rhs)
{
    sql3_ = rhs.sql3_;
    rhs.sql3_ = nullptr;
}

SQLiteStorage& SQLiteStorage::operator=(SQLiteStorage&& rhs)
{
    if (&rhs != this) {
        sql3_ = rhs.sql3_;
        rhs.sql3_ = nullptr;
    }
    return *this;
}

void SQLiteStorage::Save(VDFRecordPack const& pack)
{
    int64_t vdf_id { 0 };
    {
        StmtWrap stmt(sql3_, "insert into vdf_record (timestamp, challenge, calculated) values (?, ?, ?)");
        stmt.Bind(1, pack.record.timestamp);
        stmt.Bind(2, pack.record.challenge);
        stmt.Bind(3, pack.record.calculated);
        stmt.Run();
        vdf_id = stmt.GetLastInsertedRowID();
    }
    assert(vdf_id != 0);

    for (auto const& req : pack.requests) {
        StmtWrap stmt(sql3_,
                "insert into vdf_requests (vdf_id, iters, estimated_seconds, group_hash, netspace) values (?, ?, ?, ?, "
                "?)");
        stmt.Bind(1, vdf_id);
        stmt.Bind(2, req.iters);
        stmt.Bind(3, req.estimated_seconds);
        stmt.Bind(4, req.group_hash);
        stmt.Bind(5, req.netspace);
        stmt.Run();
    }
}

std::tuple<VDFRecord, bool> SQLiteStorage::QueryRecord(int64_t vdf_id) const
{
    VDFRecord record;
    StmtWrap stmt(sql3_, "select vdf_id, timestamp, challenge, calculated from vdf_record where vdf_id = ?");
    stmt.Bind(1, vdf_id);
    if (!stmt.StepNext()) {
        return std::make_tuple(record, false);
    }
    record.id = vdf_id;
    record.timestamp = stmt.GetColumnInt64(1);
    record.challenge = stmt.GetColumnUint256(2);
    record.calculated = stmt.GetColumnInt64(3);
    return std::make_tuple(record, true);
}

std::tuple<VDFRecord, bool> SQLiteStorage::QueryLastRecord() const
{
    VDFRecord record;
    StmtWrap stmt(
            sql3_, "select vdf_id, timestamp, challenge, calculated from vdf_record order by vdf_id desc limit 1");
    if (!stmt.StepNext()) {
        return std::make_tuple(record, false);
    }
    record.id = stmt.GetColumnInt64(0);
    record.timestamp = stmt.GetColumnInt64(1);
    record.challenge = stmt.GetColumnUint256(2);
    record.calculated = stmt.GetColumnInt64(3);
    return std::make_tuple(record, true);
}

std::vector<VDFRecord> SQLiteStorage::QueryRecords(uint32_t begin_timestamp, uint32_t end_timestamp) const
{
    std::vector<VDFRecord> records;
    StmtWrap stmt(sql3_,
            "select vdf_id, timestamp, challenge, calculated from vdf_record where timestamp >= ? and timestamp <= ?");
    stmt.Bind(1, begin_timestamp);
    stmt.Bind(2, end_timestamp);
    while (stmt.StepNext()) {
        VDFRecord record;
        record.id = stmt.GetColumnInt64(0);
        record.timestamp = stmt.GetColumnInt64(1);
        record.challenge = stmt.GetColumnUint256(2);
        record.calculated = stmt.GetColumnInt64(3);
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<VDFRequest> SQLiteStorage::QueryRequests(int64_t vdf_id) const
{
    std::vector<VDFRequest> requests;
    StmtWrap stmt(
            sql3_, "select vdf_id, iters, estimated_seconds, group_hash, netspace from vdf_requests where vdf_id = ?");
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

void SQLiteStorage::BeginTransaction()
{
    ExecuteSQL(sql3_, "begin transaction");
}

void SQLiteStorage::CommitTransaction()
{
    ExecuteSQL(sql3_, "commit transaction");
}
