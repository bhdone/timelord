#include "sqlite_storage.h"

#include <sqlite3.h>

#include <tinyformat.h>

#include <filesystem>
namespace fs = std::filesystem;

void ExecuteSQL(sqlite3* sql3, std::string_view sql)
{
    int res = sqlite3_exec(sql3, sql.data(), nullptr, nullptr, nullptr);
    if (res != SQLITE_OK) {
        throw std::runtime_error(tinyformat::format("cannot execute sql: %s", sql));
    }
}

SQLiteStorage::SQLiteStorage(std::string_view file_path)
{
    sqlite3* sql3;
    if (fs::exists(file_path) && fs::is_regular_file(file_path)) {
        if (sqlite3_open(file_path.data(), &sql3) != SQLITE_OK) {
            throw std::runtime_error(tinyformat::format("cannot open existing sqlite database %s", file_path));
        }
    } else {
        if (sqlite3_open(file_path.data(), &sql3) != SQLITE_OK) {
            throw std::runtime_error(tinyformat::format("cannot create a new sqlite database %s", file_path));
        }
        // creating tables
        ExecuteSQL(sql3_.get(), "create table vdf_record (vdf_id, timestamp, challenge, calculated)");
        ExecuteSQL(sql3_.get(), "create table vdf_requests (vdf_id, iters, estimated_seconds, group_hash, netspace)");
    }
    sql3_.reset(sql3);
}

SQLiteStorage::~SQLiteStorage()
{
    if (sql3_) {
        sqlite3_close(sql3_.get());
    }
}

void SQLiteStorage::Save(VDFRecordPack const& pack) { }

std::vector<VDFRecordPack> SQLiteStorage::Query(uint32_t begin_timestamp, uint32_t end_timestamp) const { }

VDFRecordPack SQLiteStorage::QueryLast() const { }
