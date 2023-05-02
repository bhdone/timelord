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

    static Bytes MakeRandomBytes(std::size_t len)
    {
        Bytes res(len, '\0');
        for (int i = 0; i < len; ++i) {
            res[i] = random() % 256;
        }
        return res;
    }

    static VDFRecordPack GenerateRandomPack(uint32_t timestamp, uint32_t height, bool calculated)
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

    static VDFRequest GenerateRandomRequest()
    {
        VDFRequest request;
        request.vdf_id = -1;
        request.iters = random();
        request.estimated_seconds = random();
        request.group_hash = MakeRandomUInt256();
        request.total_size = random();
        return request;
    }

    static VDFResult GenerateRandomResult(uint256 challenge, uint64_t iters, int dur)
    {
        VDFResult result;
        result.challenge = std::move(challenge);
        result.iters = iters;
        result.duration = dur;
        result.witness_type = 11;
        result.y = MakeRandomBytes(100);
        result.proof = MakeRandomBytes(233);
        return result;
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

TEST_F(StorageTest, StoreAndQuery1RecWithRequestResult)
{
    VDFRecordPack pack = GenerateRandomPack(time(nullptr), 20000, false);
    pack.requests.push_back(GenerateRandomRequest());
    pack.results.push_back(GenerateRandomResult(pack.record.challenge, pack.requests[0].iters, 10000));
    EXPECT_NO_THROW({
        pack.record.vdf_id = GetPersist().Save(pack);
        for (auto& req : pack.requests) {
            req.vdf_id = pack.record.vdf_id;
        }
        GetPersist().UpdateRecordCalculated(pack.record.vdf_id, true);
        pack.record.calculated = true;
    });
    auto [last_pack, exists] = GetPersist().QueryLastRecordPack();
    EXPECT_TRUE(exists);
    EXPECT_EQ(last_pack.requests.size(), 1);
    EXPECT_EQ(last_pack.results.size(), 1);
    EXPECT_EQ(pack, last_pack);
}
