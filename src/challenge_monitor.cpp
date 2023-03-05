#include "challenge_monitor.h"

#include <filesystem>
namespace fs = std::filesystem;

ChallengeMonitor::ChallengeMonitor(asio::io_context& ioc, std::string_view url, std::string_view cookie_path,
        std::string_view rpc_user, std::string_view rpc_password, int interval_seconds)
    : ioc_(ioc)
    , timer_(ioc)
    , cookie_path_str_(cookie_path)
    , interval_seconds_(interval_seconds)
{
    if (!rpc_user.empty() && !rpc_password.empty()) {
        prpc_ = std::make_unique<RPCClient>(true, std::string(url), std::string(rpc_user), std::string(rpc_password));
    } else {
        prpc_ = std::make_unique<RPCClient>(true, std::string(url), cookie_path_str_);
    }
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
        RPCClient::Result result = prpc_->Call("querychallenge");
        uint256 challenge = Uint256FromHex(result.result["challenge"].asString());
        if (challenge != challenge_) {
            auto old_challenge = challenge_;
            challenge_ = challenge;
            if (new_challenge_handler_) {
                new_challenge_handler_(old_challenge, challenge);
            }
        }
    } catch (NetError const& e) {
        // The wallet is not available, cookie should be reload to ensure the login is correct
        if (!cookie_path_str_.empty()) {
            if (fs::exists(cookie_path_str_) && fs::is_regular_file(cookie_path_str_)) {
                prpc_->LoadCookie(cookie_path_str_);
            }
        }
    } catch (std::exception const& e) {
        PLOGE << e.what();
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
