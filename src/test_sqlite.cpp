#include <gtest/gtest.h>

#include <string>

#include <filesystem>
namespace fs = std::filesystem;

#include "sqlite_wrap.h"
#include "timelord_utils.h"

char const* SZ_TESTDB_NAME = "test-database.sqlite3";
char const* SZ_UINT256 = "8138553ff6aacccda3d29bf20ad941f9ca7966ea336eea64182c947b7a938394";

class SQLiteTest : public testing::Test
{
protected:
    void SetUp() override
    {
        if (fs::exists(SZ_TESTDB_NAME) && !fs::is_directory(SZ_TESTDB_NAME)) {
            fs::remove(SZ_TESTDB_NAME);
        }
        sqlite_ = std::make_unique<SQLite>(SZ_TESTDB_NAME);
    }

    void TearDown() override
    {
        sqlite_.reset();
        fs::remove(SZ_TESTDB_NAME);
    }

    SQLite& GetSQLite()
    {
        return *sqlite_.get();
    }

private:
    std::unique_ptr<SQLite> sqlite_;
};

TEST_F(SQLiteTest, Create)
{
    EXPECT_EQ(GetSQLite().GetStatus(), SQLite::Status::Created);
}

TEST_F(SQLiteTest, CreateTables)
{
    EXPECT_NO_THROW({ GetSQLite().ExecuteSQL("create table hello (world)"); });
}

TEST_F(SQLiteTest, StoreValue_String)
{
    EXPECT_NO_THROW({ GetSQLite().ExecuteSQL("create table hello (world)"); });

    EXPECT_NO_THROW({
        auto stmt = GetSQLite().Prepare("insert into hello (world) values (?)");
        stmt.Bind(1, "string value");
        stmt.Run();
    });

    EXPECT_NO_THROW({
        auto stmt = GetSQLite().Prepare("select world from hello");
        EXPECT_TRUE(stmt.StepNext());
        auto str = stmt.GetColumnString(0);
        EXPECT_EQ(str, "string value");
    });
}

TEST_F(SQLiteTest, StoreValue_Int64)
{
    EXPECT_NO_THROW({ GetSQLite().ExecuteSQL("create table hello (world)"); });

    EXPECT_NO_THROW({
        auto stmt = GetSQLite().Prepare("insert into hello (world) values (?)");
        stmt.Bind(1, 1234567890);
        stmt.Run();
    });

    EXPECT_NO_THROW({
        auto stmt = GetSQLite().Prepare("select world from hello");
        EXPECT_TRUE(stmt.StepNext());
        auto val = stmt.GetColumnInt64(0);
        EXPECT_EQ(val, 1234567890);
    });
}

TEST_F(SQLiteTest, StoreValue_Real)
{
    EXPECT_NO_THROW({ GetSQLite().ExecuteSQL("create table hello (world)"); });

    EXPECT_NO_THROW({
        auto stmt = GetSQLite().Prepare("insert into hello (world) values (?)");
        stmt.Bind(1, 1.23456);
        stmt.Run();
    });

    EXPECT_NO_THROW({
        auto stmt = GetSQLite().Prepare("select world from hello");
        EXPECT_TRUE(stmt.StepNext());
        EXPECT_TRUE(stmt.IsColumnReal(0));
    });
}

TEST_F(SQLiteTest, StoreValue_Uint256)
{
    uint256 val_uint256 = Uint256FromHex(SZ_UINT256);

    EXPECT_NO_THROW({ GetSQLite().ExecuteSQL("create table hello (world)"); });

    EXPECT_NO_THROW({
        auto stmt = GetSQLite().Prepare("insert into hello (world) values (?)");
        stmt.Bind(1, val_uint256);
        stmt.Run();
    });

    EXPECT_NO_THROW({
        auto stmt = GetSQLite().Prepare("select world from hello");
        EXPECT_TRUE(stmt.StepNext());
        auto val = stmt.GetColumnUint256(0);
        EXPECT_EQ(val, val_uint256);
    });
}
