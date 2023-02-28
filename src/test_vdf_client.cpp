#include <gtest/gtest.h>

#include <plog/Log.h>

#include <memory>
#include <thread>

#include <filesystem>
namespace fs = std::filesystem;

#include <asio.hpp>

#include "vdf_client_man.h"

#include "timelord_utils.h"
#include "utils.h"

static std::string const VDF_CLIENT_PATH = "$HOME/Workspace/BitcoinHD/chiavdf/src/vdf_client";
static std::string const VDF_CLIENT_ADDR = "127.0.0.1";
static unsigned short const VDF_CLIENT_PORT = 29292;
static uint64_t VDF_TEST_ITERS = 1234567;

class VdfClientTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        pman_ = std::make_unique<VdfClientMan>(ioc_, TimeType::T, ExpandEnvPath(VDF_CLIENT_PATH), VDF_CLIENT_ADDR, VDF_CLIENT_PORT);
    }

    void TearDown() override
    {
        PLOGD << "exiting...";
        pman_->Exit();
        if (pthread_) {
            pthread_->join();
        }
        PLOGD << "exited.";
    }

    void Run(uint64_t iters)
    {
        uint256 challenge;
        MakeZero(challenge, 1);
        pman_->Run();
        pman_->CalcIters(challenge, iters);
        pthread_ = std::make_unique<std::thread>([this]() {
            ioc_.run();
        });
    }

    void SetProofReceiver(ProofReceiver proof_receiver)
    {
        pman_->SetProofReceiver(std::move(proof_receiver));
    }

private:
    asio::io_context ioc_;
    std::unique_ptr<std::thread> pthread_;
    std::unique_ptr<VdfClientMan> pman_;
};

TEST(Path, Check)
{
    EXPECT_TRUE(fs::exists(ExpandEnvPath(VDF_CLIENT_PATH)));
}

TEST_F(VdfClientTest, Base)
{
    bool proof_is_ready { false };
    std::condition_variable cv;
    std::mutex m;
    SetProofReceiver([&m, &proof_is_ready, &cv](uint256 const& challenge, Bytes const& y, Bytes const& proof, uint8_t witness_type, uint64_t iters, int duration) {
        PLOGD << "==> challenge: " << Uint256ToHex(challenge);
        PLOGD << "==> y: " << BytesToHex(y);
        PLOGD << "==> proof: " << BytesToHex(proof);
        PLOGD << "==> witness_type: " << (int)witness_type;
        PLOGD << "==> iters: " << iters;
        PLOGD << "==> duration: " << duration;
        {
            std::lock_guard<std::mutex> lg(m);
            proof_is_ready = true;
        }
        cv.notify_one();
    });
    Run(VDF_TEST_ITERS);
    std::unique_lock lk(m);
    cv.wait(lk, [&proof_is_ready]() -> bool {
        return proof_is_ready;
    });
}
