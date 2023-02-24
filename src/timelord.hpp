#ifndef TL_TIMELORD_HPP
#define TL_TIMELORD_HPP

#include <map>
#include <functional>
#include <thread>

#include <string>
#include <string_view>

#include <filesystem>
namespace fs = std::filesystem;

#include <plog/Log.h>
#include <fmt/core.h>

#include "common_types.h"

#include "front_end.hpp"
#include "vdf_client_man.h"

#include "msg_ids.h"
#include "utils.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

inline void SendMsg_Ready(fe::SessionPtr psession)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(FeMsgs::MSGID_FE_READY);
    psession->SendMessage(msg);
}

inline void SendMsg_Proof(fe::SessionPtr psession, uint256 const& challenge, Bytes const& y, Bytes const& proof, uint8_t witness_type, uint64_t iters, int duration)
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

inline void SendMsg_Stopped(fe::SessionPtr psession, uint256 const& challenge)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(FeMsgs::MSGID_FE_STOPPED);
    msg["challenge"] = Uint256ToHex(challenge);
    psession->SendMessage(msg);
}

inline void SendMsg_Status(fe::SessionPtr psession, uint256 const& challenge, uint64_t iters, int duration)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(FeMsgs::MSGID_FE_STATUS);
    msg["challenge"] = Uint256ToHex(challenge);
    msg["iters"] = iters;
    msg["duration"] = duration;
    psession->SendMessage(msg);
}

class Timelord
{
public:
    Timelord() : fe_(ioc_), vdf_client_man_(ioc_vdf_)
    {
        msg_dispatcher_.RegisterHandler(static_cast<int>(BhdMsgs::MSGID_BHD_CALC), std::bind(&Timelord::HandleMsg_Calc, this, _1, _2));
        msg_dispatcher_.RegisterHandler(static_cast<int>(BhdMsgs::MSGID_BHD_STOP), std::bind(&Timelord::HandleMsg_Stop, this, _1, _2));
        msg_dispatcher_.RegisterHandler(static_cast<int>(BhdMsgs::MSGID_BHD_QUERY), std::bind(&Timelord::HandleMsg_Query, this, _1, _2));
    }

    int Run(std::string_view addr, unsigned short port, std::string_view vdf_client_path, std::string_view vdf_client_addr, unsigned short vdf_client_port)
    {
        try {
            if (!fs::exists(vdf_client_path) || !fs::is_regular_file(vdf_client_path)) {
                throw std::runtime_error(fmt::format("the full path to `vdf_client' is incorrect, path={}", vdf_client_path));
            }
            // run a new thread to run vdf io
            auto pthread = std::make_unique<std::thread>([this]() {
                ioc_vdf_.run();
            });
            vdf_client_man_.Start(std::bind(&Timelord::HandleVdf_ProofIsReceived, this, _1, _2, _3, _4), vdf_client_path, vdf_client_addr, vdf_client_port);
            // run front end on main thread
            fe_.Run(addr, port,
                    std::bind(&Timelord::HandleSessionConnected, this, _1),
                    std::bind(&fe::MessageDispatcher::DispatchMessage, &msg_dispatcher_, _1, _2)
                    );
            PLOGI << "Timelord is running... listening " << addr << ":" << port;
            ioc_.run();
            vdf_client_man_.Stop();
            pthread->join();
        } catch (std::exception const& e) {
            PLOGE << e.what();
        }
        return 0;
    }

private:
    void HandleSessionConnected(fe::SessionPtr psession)
    {
        SendMsg_Ready(psession);
    }

    void HandleVdf_ProofIsReceived(vdf_client::Proof const& proof, uint64_t iters, uint64_t duration, uint256 challenge)
    {
        // switch to main thread
        asio::post(ioc_,
                [this, proof, iters, duration, challenge]() {
                    fe_.SendMsgToAllSessions(
                            [&proof, iters, duration, challenge](fe::SessionPtr psession) {
                                SendMsg_Proof(psession, challenge, proof.y, proof.proof, proof.witness_type, iters, duration);
                            });
                });
    }

    void HandleMsg_Calc(fe::SessionPtr psession, Json::Value const& msg)
    {
        uint256 challenge = Uint256FromHex(msg["challenge"].asString());
        uint64_t iters = msg["iters"].asInt64();
        // switch to vdf thread
        asio::post(ioc_vdf_,
                [this, challenge, iters]() {
                    vdf_client_man_.CalcIters(challenge, iters);
                });
    }

    void HandleMsg_Stop(fe::SessionPtr psession, Json::Value const& msg)
    {
        asio::post(ioc_vdf_,
                [this, challenge = Uint256FromHex(msg["challenge"].asString())]() {
                    vdf_client_man_.StopByChallenge(challenge);
                });
    }

    void HandleMsg_Query(fe::SessionPtr psession, Json::Value const& msg)
    {
        // retrieve all running challenges and proofs have been calculated
        if (!msg.isMember("category")) {
            PLOGE << "field `category` cannot be found, query status without type is not allowed";
            return;
        }
        // switch to vdf thread
        asio::post(ioc_vdf_,
                [this, category_str = msg["category"].asString(), psession]() {
                    std::unique_ptr<Json::Value> preply_msg;
                    if (category_str == "running") {
                        auto running_challenges = vdf_client_man_.GetRunningChallenges();
                        preply_msg = std::make_unique<Json::Value>(Json::arrayValue);
                        for (auto const& challenge : running_challenges) {
                            preply_msg->append(Uint256ToHex(challenge));
                        }
                    } else if (category_str == "proofs") {
                        auto cached_proofs = vdf_client_man_.GetCachedProofs();
                        preply_msg = std::make_unique<Json::Value>(Json::arrayValue);
                        for (auto const& proof : cached_proofs) {
                            Json::Value proof_vals;
                            for (auto const& proof : proof.second) {
                                Json::Value proof_val;
                                proof_val["proof"] = BytesToHex(proof.proof.proof);
                                proof_val["y"] = BytesToHex(proof.proof.y);
                                proof_val["witness_type"] = proof.proof.witness_type;
                                proof_val["iters"] = proof.iters;
                                proof_val["duration"] = proof.duration;
                                proof_vals.append(std::move(proof_val));
                            }
                            (*preply_msg)[Uint256ToHex(proof.first)] = proof_vals;
                        }
                    } else {
                        PLOGE << fmt::format("invalid category {} to query status", category_str);
                    }
                    if (preply_msg) {
                        // switch to main thread and send the message
                        asio::post(ioc_, [psession, reply_msg = *preply_msg]() { psession->SendMessage(reply_msg); });
                    }
                });
    }

    asio::io_context ioc_;
    asio::io_context ioc_vdf_;
    fe::FrontEnd fe_;
    fe::MessageDispatcher msg_dispatcher_;
    vdf_client::VdfClientMan vdf_client_man_;
};

class TimelordClient
{
public:
    using ConnectHandler = std::function<void()>;
    using ErrorHandler = fe::ErrorHandler;
    using MessageHandler = std::function<void(Json::Value const& msg)>;

    using ProofHandler =
            std::function<void(uint256 const& challenge, Bytes const& y, Bytes const& proof, int witness_type, uint64_t iters, int duration)>;

    TimelordClient() : client_(ioc_)
    {
        msg_handlers_.insert(std::make_pair(static_cast<int>(FeMsgs::MSGID_FE_PROOF),
                [this](Json::Value const& msg) {
                    if (proof_handler_) {
                        auto challenge = Uint256FromHex(msg["challenge"].asString());
                        auto y = BytesFromHex(msg["y"].asString());
                        auto proof = BytesFromHex(msg["proof"].asString());
                        auto witness_type = msg["witness_type"].asInt();
                        auto iters = msg["iters"].asInt64();
                        auto duration = msg["duration"].asInt();
                        proof_handler_(challenge, y, proof, witness_type, iters, duration);
                    }
                }));
    }

    void SetConnectHandler(ConnectHandler conn_handler)
    {
        conn_handler_ = std::move(conn_handler);
    }

    void SetErrorHandler(ErrorHandler err_handler)
    {
        err_handler_ = std::move(err_handler);
    }

    void SetProofHandler(ProofHandler proof_handler)
    {
        proof_handler_ = std::move(proof_handler);
    }

    void Calc(uint256 const& challenge, uint64_t iters)
    {
        Json::Value msg;
        msg["id"] = static_cast<Json::Int>(BhdMsgs::MSGID_BHD_CALC);
        msg["challenge"] = Uint256ToHex(challenge);
        msg["iters"] = iters;
        asio::post(ioc_,
                [this, msg]() {
                    client_.SendMessage(msg);
                });
    }

    void Shutdown()
    {
        asio::post(ioc_,
                [this]() {
                    client_.SendShutdown();
                });
    }

    int Run(std::string_view host, unsigned short port)
    {
        try {
            pthread_ = std::make_unique<std::thread>(
                    [this]() {
                        ioc_.run();
                    });
            client_.Connect(host, port,
                    std::bind(&TimelordClient::HandleConnect, this),
                    std::bind(&TimelordClient::HandleMessage, this, _1),
                    std::bind(&TimelordClient::HandleError, this, _1, _2),
                    std::bind(&TimelordClient::HandleClose, this));
            return 0;
        } catch (std::exception const& e) {
            PLOGE << e.what();
            return 1;
        }
    }

private:
    void HandleConnect()
    {
        if (conn_handler_) {
            conn_handler_();
        }
    }

    void HandleMessage(Json::Value const& msg)
    {
        auto msg_id = msg["id"].asInt();
        auto it = msg_handlers_.find(msg_id);
        if (it != std::cend(msg_handlers_)) {
            it->second(msg);
        }
    }

    void HandleError(fe::ErrorType type, std::string_view errs)
    {
        if (err_handler_) {
            err_handler_(type, errs);
        }
    }

    void HandleClose()
    {
    }

    asio::io_context ioc_;
    fe::Client client_;
    std::unique_ptr<std::thread> pthread_;
    std::map<int, MessageHandler> msg_handlers_;
    ConnectHandler conn_handler_;
    ErrorHandler err_handler_;
    ProofHandler proof_handler_;
};

#endif
