#include <gtest/gtest.h>

#include <memory>

#include <rpc_client.h>

#include "last_block_info_querier.hpp"

char const* SZ_RPC_URL = "http://127.0.0.1:18732";
char const* SZ_RPC_COOKIE_PATH = "$HOME/.btchd/testnet3/.cookie";

class BlockQuerierTest : public testing::Test
{
protected:
    void SetUp() override
    {
        rpc_ = std::make_unique<RPCClient>(true, SZ_RPC_URL, RPCLogin(ExpandEnvPath(SZ_RPC_COOKIE_PATH)));
        querier_ = std::make_unique<LastBlockInfoQuerier>(*rpc_);
    }

    void TearDown() override { }

    std::unique_ptr<RPCClient> rpc_;
    std::unique_ptr<LastBlockInfoQuerier> querier_;
};

TEST_F(BlockQuerierTest, Query)
{
    BlockInfo block_info;
    EXPECT_NO_THROW({ block_info = (*querier_)(); });
}
