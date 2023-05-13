#include <gtest/gtest.h>

#include <cstdlib>

#include <filesystem>
namespace fs = std::filesystem;

#include "local_database_keeper.hpp"
#include "local_sqlite_storage.h"

#include "vdf_pack_by_challenge_querier.hpp"

#include "test_utils.h"

using PersistDB = LocalDatabaseKeeper<LocalSQLiteStorage>;

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

    std::unique_ptr<LocalSQLiteStorage> storage_;
    std::unique_ptr<PersistDB> persist_;
};

TEST_F(StorageTest, EmptyDB)
{
    EXPECT_THROW({ VDFPackByChallengeQuerier (*storage_)(uint256()); }, std::runtime_error);
}

TEST_F(StorageTest, StoreAndQuery1Rec)
{
    VDFRecordPack pack = GenerateRandomPack(time(nullptr), 10000, false);
    EXPECT_NO_THROW({ GetPersist().Save(pack); });
    VDFRecordPack last_pack;
    EXPECT_NO_THROW({ last_pack = VDFPackByChallengeQuerier(*storage_)(pack.record.challenge); });
    EXPECT_EQ(pack, last_pack);
}

TEST_F(StorageTest, StoreAndQuery1RecWithRequestResult)
{
    VDFRecordPack pack = GenerateRandomPack(time(nullptr), 20000, false);
    pack.requests.push_back(GenerateRandomRequest(pack.record.challenge));
    EXPECT_NO_THROW({ GetPersist().Save(pack); });
    VDFRecordPack last_pack;
    EXPECT_NO_THROW({ last_pack = VDFPackByChallengeQuerier(*storage_)(pack.record.challenge); });
    EXPECT_EQ(last_pack.requests.size(), 1);
    EXPECT_EQ(pack, last_pack);
}
