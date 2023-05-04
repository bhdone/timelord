#include <gtest/gtest.h>

#include <memory>

#include <rpc_client.h>

#include "block_querier.h"

char const* SZ_RPC_URL = "http://127.0.0.1:18732";
char const* SZ_RPC_COOKIE_PATH = "$HOME/.btchd/testnet3/.cookie";

class BlockQuerierTest : public testing::Test
{
protected:
    void SetUp() override
    {
        rpc_ = std::make_unique<RPCClient>(true, SZ_RPC_URL, RPCLogin(ExpandEnvPath(SZ_RPC_COOKIE_PATH)));
        querier_ = std::make_unique<BlockQuerier>(*rpc_);
    }

    void TearDown() override { }

    std::unique_ptr<RPCClient> rpc_;
    std::unique_ptr<BlockQuerier> querier_;
};

TEST_F(BlockQuerierTest, Query)
{
    auto [block_info, succ] = querier_->QueryBlockInfo();
    EXPECT_TRUE(succ);

    auto [block_info2, succ2] = querier_->QueryBlockInfo(block_info.challenge);
    EXPECT_TRUE(succ2);

    EXPECT_EQ(block_info.challenge, block_info2.challenge);
    EXPECT_EQ(block_info.height, block_info2.height);
}
