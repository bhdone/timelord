#include "sqlite_stmt_wrap.h"

#include <tinyformat.h>

#include "timelord_utils.h"

SQLiteStmt::SQLiteStmt(sqlite3* sql3, std::string_view sql)
    : sql3_(sql3)
{
    CheckSQL(sql3, sqlite3_prepare_v2(sql3_, sql.data(), -1, &stmt_, nullptr));
}

SQLiteStmt::~SQLiteStmt()
{
    try {
        Close();
    } catch (std::exception const&) {
        // ignore exception
    }
}

void SQLiteStmt::Close()
{
    if (stmt_) {
        CheckSQL(sql3_, sqlite3_finalize(stmt_));
    }
}

SQLiteStmt::SQLiteStmt(SQLiteStmt&& rhs)
{
    stmt_ = rhs.stmt_;
    rhs.stmt_ = nullptr;
}

SQLiteStmt& SQLiteStmt::operator=(SQLiteStmt&& rhs)
{
    if (&rhs != this) {
        stmt_ = rhs.stmt_;
        rhs.stmt_ = nullptr;
    }
    return *this;
}

void SQLiteStmt::Bind(int index, std::string_view str)
{
    CheckSQL(sql3_, sqlite3_bind_text(stmt_, index, str.data(), str.size(), SQLITE_TRANSIENT));
}

void SQLiteStmt::Bind(int index, uint256 const& val)
{
    Bind(index, Uint256ToHex(val));
}

void SQLiteStmt::Bind(int index, Bytes const& val)
{
    Bind(index, BytesToHex(val));
}

void SQLiteStmt::Run()
{
    int res = sqlite3_step(stmt_);
    if (res != SQLITE_DONE) {
        char const* errmsg = sqlite3_errmsg(sql3_);
        throw std::runtime_error(tinyformat::format("failed to run sql, error: %s", errmsg));
    }
}

bool SQLiteStmt::StepNext()
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

int64_t SQLiteStmt::GetLastInsertedRowID() const
{
    return sqlite3_last_insert_rowid(sql3_);
}

int64_t SQLiteStmt::GetColumnInt64(int index) const
{
    return sqlite3_column_int64(stmt_, index);
}

std::string SQLiteStmt::GetColumnString(int index) const
{
    return reinterpret_cast<char const*>(sqlite3_column_text(stmt_, index));
}

uint256 SQLiteStmt::GetColumnUint256(int index) const
{
    return Uint256FromHex(GetColumnString(index));
}

Bytes SQLiteStmt::GetColumnBytes(int index) const
{
    return BytesFromHex(GetColumnString(index));
}

double SQLiteStmt::GetColumnReal(int index) const
{
    return sqlite3_column_double(stmt_, index);
}

bool SQLiteStmt::IsColumnReal(int index) const
{
    auto type = sqlite3_column_type(stmt_, index);
    return type == SQLITE_FLOAT;
}
