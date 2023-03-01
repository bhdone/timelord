#include <gtest/gtest.h>

#include <memory>
#include <mutex>

#include <asio.hpp>

#include "challenge_monitor.h"
#include "test_utils.h"

static char const* SZ_RPC_URL = "http://127.0.0.1:18732";
static char const* SZ_COOKIE_PATH = "$HOME/.btchd/testnet3/.cookie";
static int const INTERVAL_SECONDS = 5;

class ChallengeMonitorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        pchallenge_monitor_ = std::make_unique<ChallengeMonitor>(
                ChallengeMonitor(ioc_, SZ_RPC_URL, ExpandEnvPath(SZ_COOKIE_PATH), INTERVAL_SECONDS));
    }

    void TearDown() override
    {
        PLOGD << "exiting...";
        pchallenge_monitor_->Exit();
        pthread_->join();
        PLOGD << "exited.";
    }

    void SetNewChallengeHandler(ChallengeMonitor::NewChallengeHandler handler)
    {
        pchallenge_monitor_->SetNewChallengeHandler(std::move(handler));
    }

    void Run()
    {
        pchallenge_monitor_->Run();
        pthread_ = std::make_unique<std::thread>([this]() {
            ioc_.run();
        });
    }

private:
    asio::io_context ioc_;
    std::unique_ptr<ChallengeMonitor> pchallenge_monitor_;
    std::unique_ptr<std::thread> pthread_;
};

TEST_F(ChallengeMonitorTest, Base)
{
    bool done { false };
    std::mutex m;
    std::condition_variable cv;
    SetNewChallengeHandler([&m, &done, &cv](uint256 const& old_challenge, uint256 const& new_challenge) {
        PLOGD << "received new challenge: " << Uint256ToHex(new_challenge)
              << ", old one: " << Uint256ToHex(old_challenge);
        EXPECT_TRUE(IsZero(old_challenge));
        {
            std::lock_guard<std::mutex> lg(m);
            done = true;
            PLOGD << "unlocked";
        }
        cv.notify_one();
    });
    Run(); // new thread will be started to run the io operations
    {
        std::unique_lock<std::mutex> lk(m);
        PLOGD << "waiting for done...";
        cv.wait(lk, [&done]() {
            return done;
        });
        PLOGD << "done.";
    }
    EXPECT_TRUE(true);
}
