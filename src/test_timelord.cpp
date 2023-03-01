#include <gtest/gtest.h>

#include <thread>

#include "timelord.h"
#include "utils.h"

static char const* SZ_URL = "http://127.0.0.1:18732";
static char const* SZ_COOKIE_PATH = "$HOME/.btchd/testnet3/.cookie";
static char const* SZ_VDF_CLIENT_PATH = "$HOME/Workspace/BitcoinHD/chiavdf/src/vdf_client";
static char const* SZ_VDF_CLIENT_ADDR = "127.0.0.1";
static unsigned short const VDF_CLIENT_PORT = 19191;

static char const* SZ_TIMELORD_LISTENING_ADDR = "127.0.0.1";
static unsigned short const TIMELORD_LISTENING_PORT = 29291;

class TimelordTest : public ::testing::Test
{
public:
    TimelordTest()
        : timelord_(ioc_, SZ_URL, ExpandEnvPath(SZ_COOKIE_PATH), ExpandEnvPath(SZ_VDF_CLIENT_PATH), SZ_VDF_CLIENT_ADDR,
                VDF_CLIENT_PORT)
    {
    }

protected:
    void SetUp() override
    {
        timelord_.Run(SZ_TIMELORD_LISTENING_ADDR, TIMELORD_LISTENING_PORT);
    }

    void TearDown() override
    {
        timelord_.Exit();
        pthread_->join();
    }

    void Run()
    {
        pthread_ = std::make_unique<std::thread>([this]() {
            ioc_.run();
        });
    }

private:
    asio::io_context ioc_;
    Timelord timelord_;
    std::unique_ptr<std::thread> pthread_;
};

TEST_F(TimelordTest, baseTest)
{
    Run();
}
