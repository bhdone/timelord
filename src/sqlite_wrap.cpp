#include "sqlite_wrap.h"

#include <sqlite3.h>
#include <tinyformat.h>

#include <memory>

#include <filesystem>
namespace fs = std::filesystem;

#include "sqlite_utils.h"

#include "sqlite_stmt_wrap.h"

SQLite::SQLite(std::string_view file_path)
{
    if (fs::exists(file_path) && fs::is_regular_file(file_path)) {
        if (sqlite3_open(file_path.data(), &sql3_) != SQLITE_OK) {
            throw std::runtime_error(tinyformat::format("cannot open existing sqlite database %s", file_path));
        }
        status_ = Status::Opened;
    } else {
        if (sqlite3_open(file_path.data(), &sql3_) != SQLITE_OK) {
            throw std::runtime_error(tinyformat::format("cannot create a new sqlite database %s", file_path));
        }
        status_ = Status::Created;
    }
}

SQLite::~SQLite()
{
    if (sql3_) {
        int res = sqlite3_close(sql3_);
        if (res != SQLITE_OK) {
            // TODO: handle error here
        }
    }
}

SQLite::SQLite(SQLite&& rhs)
{
    sql3_ = rhs.sql3_;
    rhs.sql3_ = nullptr;
}

SQLite& SQLite::operator=(SQLite&& rhs)
{
    if (&rhs != this) {
        sql3_ = rhs.sql3_;
        rhs.sql3_ = nullptr;
    }
    return *this;
}

SQLite::Status SQLite::GetStatus() const
{
    return status_;
}

void SQLite::ExecuteSQL(std::string_view sql)
{
    int res = sqlite3_exec(sql3_, sql.data(), nullptr, nullptr, nullptr);
    if (res != SQLITE_OK) {
        auto err_msg = sqlite3_errmsg(sql3_);
        throw std::runtime_error(tinyformat::format("cannot execute sql: %s, err: %s", sql, err_msg));
    }
}

SQLiteStmt SQLite::Prepare(std::string_view sql)
{
    return SQLiteStmt(sql3_, sql);
}

void SQLite::BeginTransaction()
{
    ExecuteSQL("begin transaction");
}

void SQLite::CommitTransaction()
{
    ExecuteSQL("commit transaction");
}
