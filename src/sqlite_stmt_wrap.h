#ifndef sqlite_stmt_wrap_h
#define sqlite_stmt_wrap_h

#include <string_view>

#include <sqlite3.h>

#include "common_types.h"

#include "sqlite_utils.h"

class SQLiteStmt
{
public:
    SQLiteStmt(sqlite3* sql3, std::string_view sql);

    ~SQLiteStmt();

    void Close();

    SQLiteStmt(SQLiteStmt const& rhs) = delete;

    SQLiteStmt& operator=(SQLiteStmt const& rhs) = delete;

    SQLiteStmt(SQLiteStmt&& rhs);

    SQLiteStmt& operator=(SQLiteStmt&& rhs);

    void Bind(int index, std::string_view str_val);

    void Bind(int index, uint256 const& uint256_val);

    void Bind(int index, Bytes const& bytes_val);

    template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true> void Bind(int index, T int_val)
    {
        CheckSQL(sql3_, sqlite3_bind_int64(stmt_, index, int_val));
    }

    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true> void Bind(int index, T real_val)
    {
        CheckSQL(sql3_, sqlite3_bind_double(stmt_, index, real_val));
    }

    void Run();

    bool StepNext();

    int64_t GetLastInsertedRowID() const;

    int64_t GetColumnInt64(int index) const;

    std::string GetColumnString(int index) const;

    uint256 GetColumnUint256(int index) const;

    Bytes GetColumnBytes(int index) const;

    double GetColumnReal(int index) const;

    bool IsColumnReal(int index) const;

private:
    sqlite3* sql3_;
    sqlite3_stmt* stmt_ { nullptr };
};

#endif
