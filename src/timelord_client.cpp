#include "timelord_client.h"

#include "msg_ids.h"

#include "timelord_utils.h"

using std::placeholders::_1;
using std::placeholders::_2;

TimelordClient::TimelordClient(asio::io_context& ioc)
    : ioc_(ioc)
    , client_(ioc)
{
    msg_handlers_.insert(std::make_pair(static_cast<int>(TimelordMsgs::PROOF), [this](Json::Value const& msg) {
        if (proof_receiver_) {
            auto challenge = Uint256FromHex(msg["challenge"].asString());
            vdf_client::ProofDetail detail;
            detail.y = BytesFromHex(msg["y"].asString());
            detail.proof = BytesFromHex(msg["proof"].asString());
            detail.witness_type = msg["witness_type"].asInt();
            detail.iters = msg["iters"].asInt64();
            detail.duration = msg["duration"].asInt();
            proof_receiver_(challenge, detail);
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

void TimelordClient::SetProofReceiver(vdf_client::ProofReceiver proof_receiver)
{
    proof_receiver_ = std::move(proof_receiver);
}

void TimelordClient::Calc(uint256 const& challenge, uint64_t iters)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(TimelordClientMsgs::CALC);
    msg["challenge"] = Uint256ToHex(challenge);
    msg["iters"] = iters;
    client_.SendMessage(msg);
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

void TimelordClient::HandleError(FrontEndSessionErrorType type, std::string_view errs)
{
    if (err_handler_) {
        err_handler_(type, errs);
    }
}

void TimelordClient::HandleClose() { }
