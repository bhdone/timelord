#ifndef sqlite_stmt_wrap_h
#define sqlite_stmt_wrap_h

#include <string_view>

#include <sqlite3.h>

#include "common_types.h"

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

    void Bind(int index, std::string_view str);

    void Bind(int index, int64_t val);

    void Bind(int index, uint256 const& val);

    void Bind(int index, Bytes const& val);

    void Run();

    bool StepNext();

    int64_t GetLastInsertedRowID() const;

    int64_t GetColumnInt64(int index) const;

    std::string GetColumnString(int index) const;

    uint256 GetColumnUint256(int index) const;

    Bytes GetColumnBytes(int index) const;

private:
    sqlite3* sql3_;
    sqlite3_stmt* stmt_ { nullptr };
};

#endif
