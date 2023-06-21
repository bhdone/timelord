#include <gtest/gtest.h>

#include <plog/Log.h>

#include <memory>
#include <thread>

#include <filesystem>
namespace fs = std::filesystem;

#include "asio_defs.hpp"

#include "vdf_client_man.h"

#include "timelord_utils.h"
#include "utils.h"

static std::string const VDF_CLIENT_PATH = "$HOME/vdf_client";
static std::string const VDF_CLIENT_ADDR = "127.0.0.1";
static unsigned short const VDF_CLIENT_PORT = 29292;
static uint64_t VDF_TEST_ITERS = 1234567;
static uint64_t VDF_ITERS_PER_SEC = 100000;

class VdfClientTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        pman_ = std::make_unique<vdf_client::VdfClientMan>(ioc_, vdf_client::TimeType::N, ExpandEnvPath(VDF_CLIENT_PATH), VDF_CLIENT_ADDR, VDF_CLIENT_PORT);
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

    void PutIters(uint256 const& challenge, uint64_t iters)
    {
        pman_->CalcIters(challenge, iters);
    }

    void Run()
    {
        pman_->Run();
        pthread_ = std::make_unique<std::thread>([this]() {
            ioc_.run();
        });
    }

    void SetProofReceiver(vdf_client::ProofReceiver proof_receiver)
    {
        pman_->SetProofReceiver(std::move(proof_receiver));
    }

private:
    asio::io_context ioc_;
    std::unique_ptr<std::thread> pthread_;
    std::unique_ptr<vdf_client::VdfClientMan> pman_;
};

TEST(Path, Check)
{
    EXPECT_TRUE(fs::exists(ExpandEnvPath(VDF_CLIENT_PATH)));
}

TEST_F(VdfClientTest, Base)
{
    bool proof_is_ready { false };
    std::map<uint256, std::vector<vdf_client::ProofDetail>> recv_proofs;
    int num_of_waiting_proofs { 1 };
    std::condition_variable cv;
    std::mutex m;
    SetProofReceiver([&m, &proof_is_ready, &cv, &recv_proofs, &num_of_waiting_proofs](uint256 const& challenge, vdf_client::ProofDetail const& detail) {
        PLOGD << "==> challenge: " << Uint256ToHex(challenge);
        PLOGD << "==> y: " << BytesToHex(detail.y);
        PLOGD << "==> proof: " << BytesToHex(detail.proof);
        PLOGD << "==> witness_type: " << (int)detail.witness_type;
        PLOGD << "==> iters: " << detail.iters;
        PLOGD << "==> duration: " << detail.duration;
        PLOGD << "==> iters/sec: " << detail.iters / detail.duration;
        {
            std::lock_guard<std::mutex> lg(m);
            auto it = recv_proofs.find(challenge);
            if (it == std::cend(recv_proofs)) {
                recv_proofs.insert(std::make_pair(challenge, std::vector<vdf_client::ProofDetail> { detail }));
            } else {
                it->second.push_back(detail);
            }
            --num_of_waiting_proofs;
            proof_is_ready = num_of_waiting_proofs == 0;
        }
        cv.notify_one();
    });
    uint256 challenge1, challenge2;
    MakeZero(challenge1, 1);
    PutIters(challenge1, VDF_ITERS_PER_SEC * 60 * 60);
    // MakeZero(challenge2, 2);
    // PutIters(challenge1, VDF_TEST_ITERS);
    // PutIters(challenge1, VDF_TEST_ITERS * 3);
    // PutIters(challenge2, VDF_TEST_ITERS * 1);
    Run();
    std::unique_lock lk(m);
    cv.wait(lk, [&proof_is_ready]() -> bool {
        return proof_is_ready;
    });
}
