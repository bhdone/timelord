#ifndef TL_TIMELORD_HPP
#define TL_TIMELORD_HPP

#include <functional>
#include <map>
#include <thread>

#include <string>
#include <string_view>

#include <fmt/core.h>
#include <plog/Log.h>

#include "common_types.h"

#include "challenge_monitor.h"
#include "front_end.h"
#include "vdf_client_man.h"

#include "msg_ids.h"
#include "timelord_utils.h"

class Timelord
{
    struct ChallengeRequest {
        std::weak_ptr<FrontEndSession> pweak_session;
        uint64_t iters;
    };

public:
    Timelord(asio::io_context& ioc, std::string_view url, std::string_view cookie_path, std::string_view vdf_client_path, std::string_view vdf_client_addr, unsigned short vdf_client_port);

    void Run(std::string_view addr, unsigned short port);

    void Exit();

private:
    void StartNewChallengeCalculation(uint256 const& challenge, uint64_t iters);

    void HandleNewChallenge(uint256 const& old_challenge, uint256 const& new_challenge);

    void HandleSessionConnected(FrontEndSessionPtr psession);

    void HandleVdf_ProofIsReceived(uint256 const& challenge, Bytes const& y, Bytes const& proof, uint8_t witness_type, uint64_t iters, int duration);

    void HandleMsg_Calc(FrontEndSessionPtr psession, Json::Value const& msg);

    asio::io_context& ioc_;
    FrontEnd frontend_;
    std::map<uint256, std::vector<ChallengeRequest>> challenge_reqs_;
    ChallengeMonitor challenge_monitor_;
    MessageDispatcher msg_dispatcher_;
    vdf_client::VdfClientMan vdf_client_man_;
};

class TimelordClient
{
public:
    using ConnectionHandler = std::function<void()>;
    using ErrorHandler = FrontEndErrorHandler;
    using MessageHandler = std::function<void(Json::Value const& msg)>;

    explicit TimelordClient(asio::io_context& ioc);

    void SetConnectionHandler(ConnectionHandler conn_handler);

    void SetErrorHandler(ErrorHandler err_handler);

    void SetProofReceiver(vdf_client::ProofReceiver proof_receiver);

    void Calc(uint256 const& challenge, uint64_t iters);

    void Connect(std::string_view host, unsigned short port);

    void Exit();

    void RequestServiceShutdown();

private:
    void HandleConnect();

    void HandleMessage(Json::Value const& msg);

    void HandleError(FrontEndSessionErrorType type, std::string_view errs);

    void HandleClose();

    asio::io_context& ioc_;
    FrontEndClient client_;
    std::unique_ptr<std::thread> pthread_;
    std::map<int, MessageHandler> msg_handlers_;
    ConnectionHandler conn_handler_;
    ErrorHandler err_handler_;
    vdf_client::ProofReceiver proof_receiver_;
};

#endif
