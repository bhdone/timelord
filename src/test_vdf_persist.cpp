#include <gtest/gtest.h>

#include <cstdlib>

#include <filesystem>
namespace fs = std::filesystem;

#include "local_sqlite_storage.h"
#include "vdf_persist_data.h"

using PersistDB = VDFPersistOperator<LocalSQLiteStorage>;

char const* SZ_VDF_DB_NAME = "vdf.sqlite3";

class StorageTest : public testing::Test
{
protected:
    void SetUp() override
    {
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

    static uint256 MakeRandomUInt256()
    {
        uint256 res;
        for (int i = 0; i < 256 / 8; ++i) {
            res[i] = random() % 256;
        }
        return res;
    }

    VDFRecordPack GenerateRandomPack(uint32_t timestamp, uint32_t height, bool calculated) const
    {
        std::srand(time(nullptr));
        VDFRecordPack pack;
        pack.record.vdf_id = -1;
        pack.record.timestamp = timestamp;
        pack.record.challenge = MakeRandomUInt256();
        pack.record.height = height;
        pack.record.calculated = calculated;
        return pack;
    }

private:
    std::unique_ptr<LocalSQLiteStorage> storage_;
    std::unique_ptr<PersistDB> persist_;
};

TEST_F(StorageTest, EmptyDB)
{
    EXPECT_NO_THROW({
        bool exists;
        std::tie(std::ignore, exists) = GetPersist().QueryLastRecordPack();
        EXPECT_FALSE(exists);
    });
}

TEST_F(StorageTest, StoreAndQuery1Rec)
{
    VDFRecordPack pack = GenerateRandomPack(time(nullptr), 10000, false);
    EXPECT_NO_THROW({ pack.record.vdf_id = GetPersist().Save(pack); });

    auto [last_pack, exists] = GetPersist().QueryLastRecordPack();
    EXPECT_TRUE(exists);
    EXPECT_EQ(pack, last_pack);
}
