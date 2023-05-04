#include <gtest/gtest.h>

#include <cstdlib>

#include <filesystem>
namespace fs = std::filesystem;

#include "local_sqlite_storage.h"
#include "vdf_persist_data.hpp"

#include "test_utils.h"

using PersistDB = VDFPersistOperator<LocalSQLiteStorage>;

char const* SZ_VDF_DB_NAME = "vdf.sqlite3";

class StorageTest : public testing::Test
{
protected:
    void SetUp() override
    {
        if (fs::is_regular_file(SZ_VDF_DB_NAME)) {
            std::remove(SZ_VDF_DB_NAME);
        }
        storage_ = std::make_unique<LocalSQLiteStorage>(SZ_VDF_DB_NAME);
        persist_ = std::make_unique<PersistDB>(*storage_);
    }

    void TearDown() override
    {
        storage_.reset();
        fs::remove(SZ_VDF_DB_NAME);
    }

    PersistDB& GetPersist()
    {
        return *persist_;
    }

private:
    std::unique_ptr<LocalSQLiteStorage> storage_;
    std::unique_ptr<PersistDB> persist_;
};

TEST_F(StorageTest, EmptyDB)
{
    EXPECT_NO_THROW({
        bool exists;
        std::tie(std::ignore, exists) = GetPersist().QueryRecordPack(uint256());
        EXPECT_FALSE(exists);
    });
}

TEST_F(StorageTest, StoreAndQuery1Rec)
{
    VDFRecordPack pack = GenerateRandomPack(time(nullptr), 10000, false);
    EXPECT_NO_THROW({ GetPersist().Save(pack); });
    auto [last_pack, exists] = GetPersist().QueryRecordPack(pack.record.challenge);
    EXPECT_TRUE(exists);
    EXPECT_EQ(pack, last_pack);
}

TEST_F(StorageTest, StoreAndQuery1RecWithRequestResult)
{
    VDFRecordPack pack = GenerateRandomPack(time(nullptr), 20000, false);
    pack.requests.push_back(GenerateRandomRequest(pack.record.challenge));
    pack.results.push_back(GenerateRandomResult(pack.record.challenge, pack.requests[0].iters, 10000));
    EXPECT_NO_THROW({ GetPersist().Save(pack); });
    auto [last_pack, exists] = GetPersist().QueryRecordPack(pack.record.challenge);
    EXPECT_TRUE(exists);
    EXPECT_EQ(last_pack.requests.size(), 1);
    EXPECT_EQ(last_pack.results.size(), 1);
    EXPECT_EQ(pack, last_pack);
}
