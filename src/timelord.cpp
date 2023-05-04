#include "timelord.h"

#include <filesystem>
namespace fs = std::filesystem;

#include <tinyformat.h>

using boost::system::error_code;

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

static int const SECS_TO_WAIT_BEFORE_CLOSE_VDF = 5;

void LogNetspace(uint256 const& group_hash, uint64_t total_size, uint64_t sum_size)
{
    PLOGI << tinyformat::format("netspace from group_hash: %s, curr %s TB, total %s TB", Uint256ToHex(group_hash), MakeNumberTBStr(total_size), MakeNumberTBStr(sum_size));
}

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

void SendMsg_CalcReply(FrontEndSessionPtr psession, bool calculating, uint256 const& challenge, std::optional<vdf_client::ProofDetail> detail)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(TimelordMsgs::CALC_REPLY);
    msg["calculating"] = calculating;
    msg["challenge"] = Uint256ToHex(challenge);
    if (detail.has_value()) {
        msg["y"] = BytesToHex(detail->y);
        msg["proof"] = BytesToHex(detail->proof);
        msg["witness_type"] = static_cast<int>(detail->witness_type);
        msg["iters"] = detail->iters;
        msg["duration"] = detail->duration;
    }
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
    PLOGD << tinyformat::format("received: %s", MsgIdToString(static_cast<TimelordClientMsgs>(id)));
    auto it = handlers_.find(id);
    if (it == std::cend(handlers_)) {
        return;
    }
    it->second(psession, msg);
}

Timelord::Timelord(asio::io_context& ioc, RPCClient& rpc, std::string_view vdf_client_path, std::string_view vdf_client_addr, unsigned short vdf_client_port, VDFSQLitePersistOperator& persist_operator)
    : ioc_(ioc)
    , persist_operator_(persist_operator)
    , challenge_monitor_(ioc_, rpc, 3)
    , frontend_(ioc)
    , vdf_client_man_(ioc_, vdf_client::TimeType::N, ExpandEnvPath(std::string(vdf_client_path)), vdf_client_addr, vdf_client_port)
{
    PLOGD << "Timelord is created with " << vdf_client_addr << ":" << vdf_client_port << ", vdf=" << vdf_client_path << " listening " << vdf_client_addr << ":" << vdf_client_port;
    auto full_path = ExpandEnvPath(std::string(vdf_client_path));
    if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
        throw std::runtime_error(fmt::format("the full path to `vdf_client' is incorrect, path={}", vdf_client_path));
    }
    vdf_client_man_.SetProofReceiver(std::bind(&Timelord::HandleVdfClient_ProofIsReceived, this, _1, _2));
    challenge_monitor_.SetNewChallengeHandler(std::bind(&Timelord::HandleChallengeMonitor_NewChallenge, this, _1, _2, _3));
    msg_dispatcher_.RegisterHandler(static_cast<int>(TimelordClientMsgs::CALC), std::bind(&Timelord::HandleFrontEnd_SessionRequestChallenge, this, _1, _2));
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
        error_code ignored_ec;
        ptimer->cancel(ignored_ec);
    }
    vdf_client_man_.Exit();
    challenge_monitor_.Exit();
    frontend_.Exit();
}

void Timelord::HandleChallengeMonitor_NewChallenge(uint256 const& old_challenge, uint256 const& new_challenge, int height)
{
    PLOGI << "challenge is changed to " << Uint256ToHex(new_challenge);
    netspace_.clear();
    auto ptimer = std::make_shared<asio::steady_timer>(ioc_);
    ptimer->expires_after(std::chrono::seconds(SECS_TO_WAIT_BEFORE_CLOSE_VDF));
    ptimer->async_wait([this, ptimer, challenge = old_challenge](error_code const& ec) {
        ptimer_wait_close_vdf_set_.erase(ptimer);
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                PLOGE << "timer: " << ec.message();
            }
            // canceled
            return;
        }
        PLOGD << "stop vdf_client (challenge=" << Uint256ToHex(challenge) << ")";
        vdf_client_man_.StopByChallenge(challenge);
    });
    ptimer_wait_close_vdf_set_.insert(std::move(ptimer));

    // the challenge must be calculated as soon as possible
    vdf_client_man_.CalcIters(new_challenge, 100000 * 60 * 60);

    // append new record to local database
    VDFRecord record;
    record.timestamp = time(nullptr);
    record.challenge = new_challenge;
    record.height = height;
    persist_operator_.AppendRecord(record);

    // need to deliver iters to vdf_client?
    auto it = challenge_reqs_.find(new_challenge);
    if (it != std::cend(challenge_reqs_)) {
        PLOGI << "delivering total " << it->second.size() << " saved request(s)";
        for (auto req : it->second) {
            vdf_client_man_.CalcIters(new_challenge, req.iters);
            // save the request to local database
            uint64_t sum_size;
            bool newly;
            std::tie(sum_size, newly) = AddAndSumNetspace(req.group_hash, req.total_size);
            if (newly) {
                LogNetspace(req.group_hash, req.total_size, sum_size);
                // update to local database
                VDFRequest request;
                request.challenge = new_challenge;
                request.iters = req.iters;
                request.estimated_seconds = iters_per_sec_ > 0 ? req.iters / iters_per_sec_ : 0;
                request.group_hash = req.group_hash;
                request.total_size = req.total_size;
                persist_operator_.AppendRequest(request);
            }
        }
    }
}

void Timelord::HandleFrontEnd_NewSessionConnected(FrontEndSessionPtr psession)
{
    PLOGD << "session count " << frontend_.GetNumOfSessions();
    SendMsg_Ready(psession);
}

void Timelord::HandleFrontEnd_SessionError(FrontEndSessionPtr psession, FrontEndSessionErrorType type, std::string_view errs)
{
    PLOGD << "session error occurs: " << errs << ", session count " << frontend_.GetNumOfSessions();
}

void Timelord::HandleFrontEnd_SessionRequestChallenge(FrontEndSessionPtr psession, Json::Value const& msg)
{
    uint256 challenge = Uint256FromHex(msg["challenge"].asString());
    uint64_t iters = msg["iters"].asInt64();

    if (iters == 0) {
        SendMsg_CalcReply(psession, false, challenge, {});
        return;
    }

    auto detail = vdf_client_man_.QueryExistingProof(challenge, iters);
    if (detail.has_value()) {
        PLOGD << tinyformat::format("the proof already exists, just send it back to miner, challenge: (iters=%s)%s", FormatNumberStr(std::to_string(iters)), Uint256ToHex(challenge));
        SendMsg_CalcReply(psession, false, challenge, detail);
        return;
    }

    PLOGD << "challenge: (iters=" << FormatNumberStr(std::to_string(iters)) << ")" << Uint256ToHex(challenge);

    uint256 group_hash;
    uint64_t total_size { 0 };
    if (msg.isMember("netspace") && msg["netspace"].isObject()) {
        Json::Value netspace = msg["netspace"];
        group_hash = Uint256FromHex(netspace["group_hash"].asString());
        total_size = netspace["total_size"].asUInt64();
    }

    // mark the session that it is related with the challenge
    auto it = challenge_reqs_.find(challenge);
    if (it == std::cend(challenge_reqs_)) {
        ChallengeRequestSession entry;
        entry.pweak_session = std::weak_ptr(psession);
        entry.iters = iters;
        entry.group_hash = group_hash;
        entry.total_size = total_size;
        // mark session with the challenge
        challenge_reqs_.insert(std::make_pair(challenge, std::vector<ChallengeRequestSession> { entry }));
    } else {
        // find the session first
        auto it2 = std::find_if(std::begin(it->second), std::end(it->second), [psession, iters](ChallengeRequestSession const& req) {
            return req.pweak_session.lock() == psession && req.iters == iters;
        });
        if (it2 == std::end(it->second)) {
            // mark session with the challenge
            it->second.push_back({ std::weak_ptr(psession), iters, group_hash, total_size });
        }
    }

    // reject when the challenge doesn't match
    if (challenge != challenge_monitor_.GetCurrentChallenge()) {
        PLOGD << "the challenge doesn't match, but the request is saved";
        SendMsg_CalcReply(psession, false, challenge, {});
        return;
    }

    if (total_size > 0) {
        uint64_t sum_size;
        bool newly;
        std::tie(sum_size, newly) = AddAndSumNetspace(group_hash, total_size);
        if (newly) {
            LogNetspace(group_hash, total_size, sum_size);
            // update to local database
            VDFRequest request;
            request.challenge = challenge;
            request.iters = iters;
            request.estimated_seconds = iters_per_sec_ > 0 ? iters / iters_per_sec_ : 0;
            request.group_hash = group_hash;
            request.total_size = total_size;
            persist_operator_.AppendRequest(request);
        }
    }

    vdf_client_man_.CalcIters(challenge, iters);
    SendMsg_CalcReply(psession, true, challenge, {});
}

void Timelord::HandleVdfClient_ProofIsReceived(uint256 const& challenge, vdf_client::ProofDetail const& detail)
{
    // first, we save it to local database
    VDFResult result;
    result.challenge = challenge;
    result.iters = detail.iters;
    result.y = detail.y;
    result.proof = detail.proof;
    result.witness_type = detail.witness_type;
    result.duration = detail.duration;
    persist_operator_.AppendResult(result);

    // calculate the VDF speed
    if (detail.duration == 0) {
        PLOGI << "proof is received from vdf_client, iters=" << detail.iters << ", n/a iters/second";
    } else {
        iters_per_sec_ = detail.iters / detail.duration;
        PLOGI << "proof is received from vdf_client, iters=" << detail.iters << ", " << (iters_per_sec_ / 1000) << "k iters/second";
    }
    // find the related session
    auto it = challenge_reqs_.find(challenge);
    if (it == std::cend(challenge_reqs_)) {
        // the proof is ready, but the session which requests for the proof cannot be found
        PLOGE << "the session relates to the proof cannot be found";
        return;
    }
    bool more_req { false };
    int sent_count { 0 };
    for (auto const& req : it->second) {
        if (req.iters <= detail.iters) {
            auto psession = req.pweak_session.lock();
            if (psession) {
                PLOGI << tinyformat::format("sending proof to session %s...", AddressToString(psession.get()));
                SendMsg_Proof(psession, challenge, detail);
                ++sent_count;
            } else {
                PLOGE << "session is lost";
            }
        } else {
            more_req = true;
        }
    }
    PLOGI << tinyformat::format("sent proof count %d", sent_count);
    if (!more_req) {
        // we need to close this vdf_client
        PLOGI << "no more request, close related vdf_client";
        vdf_client_man_.StopByChallenge(challenge);
    }
}

std::tuple<uint64_t, bool> Timelord::AddAndSumNetspace(uint256 const& group_hash, uint64_t total_size)
{
    auto it = netspace_.find(group_hash);
    bool newly = it == std::cend(netspace_);
    if (newly) {
        netspace_.insert(std::make_pair(group_hash, total_size));
    }
    // count the size every time the netspace is sent
    uint64_t sum_size { 0 };
    for (auto pa : netspace_) {
        sum_size += pa.second;
    }
    return std::make_tuple(sum_size, newly);
}
