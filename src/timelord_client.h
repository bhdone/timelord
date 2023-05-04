#ifndef TIMELORD_CLIENT_H
#define TIMELORD_CLIENT_H

#include <functional>

#include "asio_defs.hpp"

#include <json/value.h>

#include "frontend_client.h"
#include "vdf_client_man.h"

class TimelordClient
{
public:
    using ConnectionHandler = std::function<void()>;
    using ErrorHandler = std::function<void(FrontEndSessionErrorType type, std::string_view errs)>;
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
    std::map<int, MessageHandler> msg_handlers_;
    ConnectionHandler conn_handler_;
    ErrorHandler err_handler_;
    vdf_client::ProofReceiver proof_receiver_;
};

#endif
