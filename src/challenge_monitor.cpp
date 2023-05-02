#include "challenge_monitor.h"

#include <plog/Log.h>

ChallengeMonitor::ChallengeMonitor(asio::io_context& ioc, std::string_view url, RPCLogin login, int interval_seconds)
    : ioc_(ioc)
    , timer_(ioc)
    , rpc_(true, std::string(url), std::move(login))
    , interval_seconds_(interval_seconds)
{
    MakeZero(challenge_, 0);
}

uint256 const& ChallengeMonitor::GetCurrentChallenge() const
{
    return challenge_;
}

void ChallengeMonitor::SetNewChallengeHandler(NewChallengeHandler handler)
{
    new_challenge_handler_ = std::move(handler);
}

void ChallengeMonitor::Run()
{
    DoQueryNext();
}

void ChallengeMonitor::Exit()
{
    std::error_code ignored_ec;
    timer_.cancel(ignored_ec);
}

void ChallengeMonitor::QueryChallenge()
{
    try {
        RPCClient::Result result = rpc_.Call("querychallenge");
        uint256 challenge = Uint256FromHex(result.result["challenge"].get_str());
        int height = result.result["target_height"].get_int();
        if (challenge != challenge_) {
            auto old_challenge = challenge_;
            challenge_ = challenge;
            if (new_challenge_handler_) {
                new_challenge_handler_(old_challenge, challenge, height);
            }
        }
    } catch (NetError const& e) {
        PLOGE << "NetError: " << e.what();
    } catch (std::exception const& e) {
        PLOGE << "exception: " << e.what();
    }
}

void ChallengeMonitor::DoQueryNext()
{
    // ready
    QueryChallenge();
    timer_.expires_after(std::chrono::seconds(interval_seconds_));
    timer_.async_wait([this](std::error_code const& ec) {
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                PLOGE << ec.message();
            } else {
                PLOGD << "timer aborted";
            }
            return;
        }
        // next
        DoQueryNext();
    });
}
