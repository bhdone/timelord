#include "challenge_monitor.h"

ChallengeMonitor::ChallengeMonitor(asio::io_context& ioc, std::string url, std::string cookie_path, int interval_seconds, NewChallengeHandler handler)
    : ioc_(ioc)
    , timer_(ioc)
    , rpc_(true, url, cookie_path)
    , interval_seconds_(interval_seconds)
    , new_challenge_handler_(std::move(handler))
{
}

void ChallengeMonitor::Start()
{
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
        // ready
        QueryChallenge();
        // next
        Start();
    });
}

void ChallengeMonitor::Stop()
{
    std::error_code ignored_ec;
    timer_.cancel(ignored_ec);
}

void ChallengeMonitor::QueryChallenge()
{
    try {
        RPCClient::Result result = rpc_.Call("querychallenge");
        uint256 challenge = Uint256FromHex(result.result["challenge"].asString());
        if (challenge != challenge_) {
            if (new_challenge_handler_) {
                new_challenge_handler_(challenge_, challenge);
            }
            challenge_ = challenge;
        }
    } catch (std::exception const& e) {
        PLOGE << e.what();
    }
}
