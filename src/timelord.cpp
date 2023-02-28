#include "timelord.h"

#include <filesystem>
namespace fs = std::filesystem;

using std::placeholders::_1;
using std::placeholders::_2;

void SendMsg_Ready(SessionPtr psession)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(FeMsgs::MSGID_FE_READY);
    psession->SendMessage(msg);
}

void SendMsg_Proof(SessionPtr psession, uint256 const& challenge, Bytes const& y, Bytes const& proof, uint8_t witness_type, uint64_t iters, int duration)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(FeMsgs::MSGID_FE_PROOF);
    msg["challenge"] = Uint256ToHex(challenge);
    msg["y"] = BytesToHex(y);
    msg["proof"] = BytesToHex(proof);
    msg["witness_type"] = static_cast<Json::Int>(witness_type);
    msg["iters"] = iters;
    msg["duration"] = duration;
    psession->SendMessage(msg);
}

Timelord::Timelord(asio::io_context& ioc, std::string_view url, std::string_view cookie_path, std::string_view vdf_client_path, std::string_view vdf_client_addr, unsigned short vdf_client_port)
    : ioc_(ioc)
    , challenge_monitor_(ioc_, url, cookie_path, 3)
    , frontend_(ioc)
    , vdf_client_man_(ioc_, TimeType::N, vdf_client_path, vdf_client_addr, vdf_client_port)
{
    PLOGD << "Timelord is created with " << vdf_client_addr << ":" << vdf_client_port << ", vdf=" << vdf_client_path << " listening " << vdf_client_addr << ":" << vdf_client_port;
    if (!fs::exists(vdf_client_path) || !fs::is_regular_file(vdf_client_path)) {
        throw std::runtime_error(fmt::format("the full path to `vdf_client' is incorrect, path={}", vdf_client_path));
    }
    challenge_monitor_.SetNewChallengeHandler(std::bind(&Timelord::HandleNewChallenge, this, _1, _2));
    msg_dispatcher_.RegisterHandler(static_cast<int>(BhdMsgs::MSGID_BHD_CALC), std::bind(&Timelord::HandleMsg_Calc, this, _1, _2));
    PLOGD << "Initialized.";
}

void Timelord::Run(std::string_view addr, unsigned short port)
{
    // run a new thread to run vdf io
    vdf_client_man_.Run();
    PLOGD << "VDF manager started.";
    // run challange monitor on main thread
    challenge_monitor_.Run();
    PLOGD << "Challenge monitor started.";
    // run front end on main thread
    frontend_.Run(addr, port);
    frontend_.SetConnectionHandler(std::bind(&Timelord::HandleSessionConnected, this, _1));
    frontend_.SetMessageHandler(std::bind(&MessageDispatcher::DispatchMessage, &msg_dispatcher_, _1, _2));
    PLOGI << "Timelord is running...";
    ioc_.run();
    // exit
    challenge_monitor_.Exit();
    vdf_client_man_.Exit();
    PLOGD << "Exit.";
}

void Timelord::Exit() { }

void Timelord::StartNewChallengeCalculation(uint256 const& challenge, uint64_t iters)
{
    // TODO need to check there is still an idle thread available
    // TODO simply exit when the challenge is already running
    asio::post(ioc_, [this, challenge, iters]() {
        vdf_client_man_.CalcIters(challenge, iters);
    });
}

void Timelord::HandleNewChallenge(uint256 const& old_challenge, uint256 const& new_challenge)
{
    PLOGD << "Challenge is changed to " << Uint256ToHex(new_challenge) << ", stop all sessions after 1 minute";
    auto ptimer = std::make_unique<asio::steady_timer>(ioc_);
    ptimer->expires_after(std::chrono::seconds(60));
    PLOGD << "All vdf calculator relates to this challenge will be stopped after 60 seconds";
    ptimer->async_wait([this, challenge = old_challenge, ptimer = std::move(ptimer)](std::error_code const& ec) {
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                PLOGE << "timer: " << ec.message();
            }
            // canceled
            return;
        }
        vdf_client_man_.StopByChallenge(challenge);
    });
}

void Timelord::HandleSessionConnected(SessionPtr psession)
{
    PLOGD << "New session is connected, sending message `ready`...";
    SendMsg_Ready(psession);
}

void Timelord::HandleVdf_ProofIsReceived(uint256 const& challenge, Bytes const& y, Bytes const& proof, uint8_t witness_type, uint64_t iters, int duration)
{
    // find the related session
    auto it = challenge_reqs_.find(challenge);
    if (it == std::cend(challenge_reqs_)) {
        // the proof is ready, but the session which requests for the proof cannot be found
        return;
    }
    for (auto const& req : it->second) {
        if (req.iters <= iters) {
            auto psession = req.pweak_session.lock();
            if (psession) {
                asio::post(ioc_, std::bind(SendMsg_Proof, psession, challenge, y, proof, witness_type, iters, duration));
            }
        }
    }
}

void Timelord::HandleMsg_Calc(SessionPtr psession, Json::Value const& msg)
{
    uint256 challenge = Uint256FromHex(msg["challenge"].asString());
    uint64_t iters = msg["iters"].asInt64();
    auto it = challenge_reqs_.find(challenge);
    if (it == std::cend(challenge_reqs_)) {
        auto pair = challenge_reqs_.insert(std::make_pair(challenge, std::vector<ChallengeRequest>()));
        pair.first->second.push_back({ std::weak_ptr(psession), iters });
    } else {
        it->second.push_back({ std::weak_ptr(psession), iters });
    }
    // switch to vdf thread
    StartNewChallengeCalculation(challenge, iters);
}

TimelordClient::TimelordClient(asio::io_context& ioc)
    : ioc_(ioc)
    , client_(ioc)
{
    msg_handlers_.insert(std::make_pair(static_cast<int>(FeMsgs::MSGID_FE_PROOF), [this](Json::Value const& msg) {
        if (proof_receiver_) {
            auto challenge = Uint256FromHex(msg["challenge"].asString());
            auto y = BytesFromHex(msg["y"].asString());
            auto proof = BytesFromHex(msg["proof"].asString());
            auto witness_type = msg["witness_type"].asInt();
            auto iters = msg["iters"].asInt64();
            auto duration = msg["duration"].asInt();
            proof_receiver_(challenge, y, proof, witness_type, iters, duration);
        }
    }));
}

void TimelordClient::SetConnectionHandler(ConnectionHandler conn_handler)
{
    conn_handler_ = std::move(conn_handler);
}

void TimelordClient::SetErrorHandler(ErrorHandler err_handler)
{
    err_handler_ = std::move(err_handler);
}

void TimelordClient::SetProofReceiver(ProofReceiver proof_receiver)
{
    proof_receiver_ = std::move(proof_receiver);
}

void TimelordClient::Calc(uint256 const& challenge, uint64_t iters)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(BhdMsgs::MSGID_BHD_CALC);
    msg["challenge"] = Uint256ToHex(challenge);
    msg["iters"] = iters;
    asio::post(ioc_, [this, msg]() {
        client_.SendMessage(msg);
    });
}

void TimelordClient::RequestServiceShutdown()
{
    client_.SendShutdown();
}

void TimelordClient::Connect(std::string_view host, unsigned short port)
{
    client_.SetConnectionHandler(std::bind(&TimelordClient::HandleConnect, this));
    client_.SetMessageHandler(std::bind(&TimelordClient::HandleMessage, this, _1));
    client_.SetErrorHandler(std::bind(&TimelordClient::HandleError, this, _1, _2));
    client_.SetCloseHandler(std::bind(&TimelordClient::HandleClose, this));
    client_.Connect(host, port);
}

void TimelordClient::Exit() { }

void TimelordClient::HandleConnect()
{
    if (conn_handler_) {
        conn_handler_();
    }
}

void TimelordClient::HandleMessage(Json::Value const& msg)
{
    auto msg_id = msg["id"].asInt();
    auto it = msg_handlers_.find(msg_id);
    if (it != std::cend(msg_handlers_)) {
        it->second(msg);
    }
}

void TimelordClient::HandleError(ErrorType type, std::string_view errs)
{
    if (err_handler_) {
        err_handler_(type, errs);
    }
}

void TimelordClient::HandleClose() { }
