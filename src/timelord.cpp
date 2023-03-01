#include "timelord.h"

#include <filesystem>
namespace fs = std::filesystem;

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

static int const SECS_TO_WAIT_BEFORE_CLOSE_VDF = 60;

void SendMsg_Ready(FrontEndSessionPtr psession)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(TimelordMsgs::READY);
    psession->SendMessage(msg);
}

void SendMsg_Proof(FrontEndSessionPtr psession, uint256 const& challenge, vdf_client::ProofDetail const& detail)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(TimelordMsgs::PROOF);
    msg["challenge"] = Uint256ToHex(challenge);
    msg["y"] = BytesToHex(detail.y);
    msg["proof"] = BytesToHex(detail.proof);
    msg["witness_type"] = static_cast<Json::Int>(detail.witness_type);
    msg["iters"] = detail.iters;
    msg["duration"] = detail.duration;
    psession->SendMessage(msg);
}

void SendMsg_Speed(FrontEndSessionPtr psession, uint64_t iters_per_sec)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(TimelordMsgs::SPEED);
    msg["iters_per_sec"] = iters_per_sec;
    psession->SendMessage(msg);
}

void MessageDispatcher::RegisterHandler(int id, Handler handler)
{
    handlers_[id] = handler;
}

void MessageDispatcher::operator()(FrontEndSessionPtr psession, Json::Value const& msg) const
{
    if (!msg.isMember("id")) {
        PLOGE << "`id' is not found from the received message";
        PLOGD << msg.toStyledString();
        return;
    }
    auto id = msg["id"].asInt();
    auto it = handlers_.find(id);
    if (it == std::cend(handlers_)) {
        return;
    }
    it->second(psession, msg);
}

Timelord::Timelord(asio::io_context& ioc, std::string_view url, std::string_view cookie_path,
        std::string_view vdf_client_path, std::string_view vdf_client_addr, unsigned short vdf_client_port)
    : ioc_(ioc)
    , challenge_monitor_(ioc_, url, cookie_path, 3)
    , frontend_(ioc)
    , vdf_client_man_(ioc_, vdf_client::TimeType::N, ExpandEnvPath(vdf_client_path), vdf_client_addr, vdf_client_port)
{
    PLOGD << "Timelord is created with " << vdf_client_addr << ":" << vdf_client_port << ", vdf=" << vdf_client_path
          << " listening " << vdf_client_addr << ":" << vdf_client_port;
    auto full_path = ExpandEnvPath(vdf_client_path);
    if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
        throw std::runtime_error(fmt::format("the full path to `vdf_client' is incorrect, path={}", vdf_client_path));
    }
    vdf_client_man_.SetProofReceiver(std::bind(&Timelord::HandleVdfClient_ProofIsReceived, this, _1, _2));
    challenge_monitor_.SetNewChallengeHandler(std::bind(&Timelord::HandleChallengeMonitor_NewChallenge, this, _1, _2));
    msg_dispatcher_.RegisterHandler(static_cast<int>(TimelordClientMsgs::CALC),
            std::bind(&Timelord::HandleFrontEnd_SessionRequestChallenge, this, _1, _2));
    msg_dispatcher_.RegisterHandler(static_cast<int>(TimelordClientMsgs::QUERY_SPEED),
            std::bind(&Timelord::HandleFrontEnd_SessionQuerySpeed, this, _1, _2));
    frontend_.SetConnectionHandler(std::bind(&Timelord::HandleFrontEnd_NewSessionConnected, this, _1));
    frontend_.SetMessageHandler(msg_dispatcher_);
    frontend_.SetErrorHandler(std::bind(&Timelord::HandleFrontEnd_SessionError, this, _1, _2, _3));
}

void Timelord::Run(std::string_view addr, unsigned short port)
{
    vdf_client_man_.Run();
    challenge_monitor_.Run();
    frontend_.Run(addr, port);
}

void Timelord::Exit()
{
    for (auto ptimer : ptimer_wait_close_vdf_set_) {
        std::error_code ignored_ec;
        ptimer->cancel(ignored_ec);
    }
    vdf_client_man_.Exit();
    challenge_monitor_.Exit();
    frontend_.Exit();
}

void Timelord::HandleChallengeMonitor_NewChallenge(uint256 const& old_challenge, uint256 const& new_challenge)
{
    PLOGD << "Challenge is changed to " << Uint256ToHex(new_challenge)
          << ", stop all related sessions (old_challenge=" << Uint256ToHex(old_challenge) << ") after "
          << SECS_TO_WAIT_BEFORE_CLOSE_VDF << " seconds";
    auto ptimer = std::make_shared<asio::steady_timer>(ioc_);
    ptimer->expires_after(std::chrono::seconds(SECS_TO_WAIT_BEFORE_CLOSE_VDF));
    ptimer->async_wait([this, ptimer, challenge = old_challenge](std::error_code const& ec) {
        ptimer_wait_close_vdf_set_.erase(ptimer);
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                PLOGE << "timer: " << ec.message();
            }
            // canceled
            return;
        }
        vdf_client_man_.StopByChallenge(challenge);
    });
    ptimer_wait_close_vdf_set_.insert(std::move(ptimer));
}

void Timelord::HandleFrontEnd_NewSessionConnected(FrontEndSessionPtr psession)
{
    SendMsg_Ready(psession);
}

void Timelord::HandleFrontEnd_SessionError(
        FrontEndSessionPtr psession, FrontEndSessionErrorType type, std::string_view errs)
{
    PLOGD << "session error occurs: " << errs;
}

void Timelord::HandleFrontEnd_SessionRequestChallenge(FrontEndSessionPtr psession, Json::Value const& msg)
{
    uint256 challenge = Uint256FromHex(msg["challenge"].asString());
    uint64_t iters = msg["iters"].asInt64();

    auto it = challenge_reqs_.find(challenge);
    if (it == std::cend(challenge_reqs_)) {
        auto pair = challenge_reqs_.insert(std::make_pair(challenge, std::vector<ChallengeRequestSession>()));
        pair.first->second.push_back({ std::weak_ptr(psession), iters });
    } else {
        it->second.push_back({ std::weak_ptr(psession), iters });
    }

    vdf_client_man_.CalcIters(challenge, iters);
}

void Timelord::HandleFrontEnd_SessionQuerySpeed(FrontEndSessionPtr psession, Json::Value const&)
{
    SendMsg_Speed(psession, iters_per_sec_);
}

void Timelord::HandleVdfClient_ProofIsReceived(uint256 const& challenge, vdf_client::ProofDetail const& detail)
{
    // calculate the VDF speed
    iters_per_sec_ = detail.iters / detail.duration;
    // find the related session
    auto it = challenge_reqs_.find(challenge);
    if (it == std::cend(challenge_reqs_)) {
        // the proof is ready, but the session which requests for the proof cannot be found
        return;
    }
    for (auto const& req : it->second) {
        if (req.iters <= detail.iters) {
            auto psession = req.pweak_session.lock();
            if (psession) {
                SendMsg_Proof(psession, challenge, detail);
            }
        }
    }
}
