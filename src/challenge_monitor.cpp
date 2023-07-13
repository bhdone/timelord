#include "challenge_monitor.h"

#include <plog/Log.h>

#include "timelord_utils.h"

using boost::system::error_code;

ChallengeMonitor::ChallengeMonitor(asio::io_context& ioc, RPCClient& rpc, int interval_seconds)
    : ioc_(ioc)
    , timer_(ioc)
    , rpc_(rpc)
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

void ChallengeMonitor::SetNewVdfReqHandler(NewVdfReqHandler handler)
{
    new_vdf_req_handler_ = std::move(handler);
}

void ChallengeMonitor::Run()
{
    DoQueryNext();
}

void ChallengeMonitor::Exit()
{
    error_code ignored_ec;
    timer_.cancel(ignored_ec);
}

void ChallengeMonitor::QueryChallenge()
{
    try {
        RPCClient::Result result = rpc_.Call("querychallenge");
        uint256 challenge = Uint256FromHex(result.result["challenge"].get_str());
        uint64_t difficulty = result.result["difficulty"].get_int64();
        int height = result.result["target_height"].get_int();
        // save vdf_reqs
        std::set<uint64_t> new_vdf_reqs;
        if (result.result.exists("vdf_reqs")) {
            auto vdf_req_objs = result.result["vdf_reqs"];
            if (vdf_req_objs.isArray()) {
                for (auto iters_obj : vdf_req_objs.getValues()) {
                    new_vdf_reqs.insert(iters_obj.get_int64());
                }
            }
        }
        if (new_vdf_reqs != vdf_reqs_) {
            vdf_reqs_ = new_vdf_reqs;
            if (new_vdf_req_handler_) {
                new_vdf_req_handler_(challenge, new_vdf_reqs);
            }
        }
        if (challenge != challenge_) {
            auto old_challenge = challenge_;
            challenge_ = challenge;
            if (new_challenge_handler_) {
                new_challenge_handler_(old_challenge, challenge, height, difficulty);
            }
        }
        status_ = Status::NO_ERROR;
        error_string_.clear();
    } catch (NetError const& e) {
        status_ = Status::RPC_ERROR;
        error_string_ = e.what();
        PLOGE << "NetError: " << e.what();
    } catch (std::exception const& e) {
        status_ = Status::OTHER_ERROR;
        error_string_ = e.what();
        PLOGE << "exception: " << e.what();
    }
}

void ChallengeMonitor::DoQueryNext()
{
    // ready
    QueryChallenge();
    timer_.expires_after(std::chrono::seconds(interval_seconds_));
    timer_.async_wait([this](error_code const& ec) {
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
