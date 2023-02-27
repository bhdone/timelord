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
        std::weak_ptr<fe::Session> pweak_session;
        uint64_t iters;
    };

public:
    Timelord(asio::io_context& ioc, std::string_view url, std::string_view cookie_path, std::string_view vdf_client_path, std::string_view vdf_client_addr, unsigned short vdf_client_port);

    void Run(std::string_view addr, unsigned short port);

    void Exit();

private:
    void StartNewChallengeCalculation(uint256 const& challenge, uint64_t iters);

    void HandleNewChallenge(uint256 const& old_challenge, uint256 const& new_challenge);

    void HandleSessionConnected(fe::SessionPtr psession);

    void HandleVdf_ProofIsReceived(uint256 const& challenge, Bytes const& y, Bytes const& proof, uint8_t witness_type, uint64_t iters, int duration);

    void HandleMsg_Calc(fe::SessionPtr psession, Json::Value const& msg);

    asio::io_context& ioc_;
    fe::FrontEnd frontend_;
    std::map<uint256, std::vector<ChallengeRequest>> challenge_reqs_;
    ChallengeMonitor challenge_monitor_;
    fe::MessageDispatcher msg_dispatcher_;
    vdf_client::VdfClientMan vdf_client_man_;
};

class TimelordClient
{
public:
    using ConnectHandler = std::function<void()>;
    using ErrorHandler = fe::ErrorHandler;
    using MessageHandler = std::function<void(Json::Value const& msg)>;

    using ProofHandler = std::function<void(uint256 const& challenge, Bytes const& y, Bytes const& proof, int witness_type, uint64_t iters, int duration)>;

    explicit TimelordClient(asio::io_context& ioc);

    void SetConnectHandler(ConnectHandler conn_handler);

    void SetErrorHandler(ErrorHandler err_handler);

    void SetProofHandler(ProofHandler proof_handler);

    void Calc(uint256 const& challenge, uint64_t iters);

    void Connect(std::string_view host, unsigned short port);

    void Exit();

    void RequestServiceShutdown();

private:
    void HandleConnect();

    void HandleMessage(Json::Value const& msg);

    void HandleError(fe::ErrorType type, std::string_view errs);

    void HandleClose();

    asio::io_context& ioc_;
    fe::Client client_;
    std::unique_ptr<std::thread> pthread_;
    std::map<int, MessageHandler> msg_handlers_;
    ConnectHandler conn_handler_;
    ErrorHandler err_handler_;
    ProofHandler proof_handler_;
};

#endif
