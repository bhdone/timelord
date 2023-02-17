#ifndef TL_TIMELORD_HPP
#define TL_TIMELORD_HPP

#include <map>
#include <functional>

#include <string_view>

#include <filesystem>
namespace fs = std::filesystem;

#include <plog/Log.h>
#include <fmt/core.h>

#include "common_types.h"

#include "front_end.hpp"
#include "msg_ids.h"
#include "utils.h"

using std::placeholders::_1;
using std::placeholders::_2;

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
    msg["challenge"] = GetHex(challenge);
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
    msg["challenge"] = GetHex(challenge);
    psession->SendMessage(msg);
}

inline void SendMsg_Status(fe::SessionPtr psession, uint256 const& challenge, uint64_t iters, int duration)
{
    Json::Value msg;
    msg["id"] = static_cast<Json::Int>(FeMsgs::MSGID_FE_STATUS);
    msg["challenge"] = GetHex(challenge);
    msg["iters"] = iters;
    msg["duration"] = duration;
    psession->SendMessage(msg);
}

class Timelord
{
public:
    Timelord() : fe_(ioc_)
    {
        msg_dispatcher_.RegisterHandler(static_cast<int>(BhdMsgs::MSGID_BHD_CALC), std::bind(&Timelord::HandleMsg_Calc, this, _1, _2));
        msg_dispatcher_.RegisterHandler(static_cast<int>(BhdMsgs::MSGID_BHD_STOP), std::bind(&Timelord::HandleMsg_Stop, this, _1, _2));
        msg_dispatcher_.RegisterHandler(static_cast<int>(BhdMsgs::MSGID_BHD_QUERY), std::bind(&Timelord::HandleMsg_Query, this, _1, _2));
    }

    int Run(std::string_view addr, unsigned short port, std::string_view vdf_client_path)
    {
        try {
            if (!fs::exists(vdf_client_path) || !fs::is_regular_file(vdf_client_path)) {
                throw std::runtime_error(fmt::format("the full path to `vdf_client' is incorrect, path={}", vdf_client_path));
            }
            fe_.Run(addr, port,
                    std::bind(&Timelord::HandleSessionConnected, this, _1),
                    std::bind(&fe::MessageDispatcher::DispatchMessage, &msg_dispatcher_, _1, _2)
                    );
            PLOGI << "Timelord is running... listening " << addr << ":" << port;
            ioc_.run();
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

    void HandleMsg_Calc(fe::SessionPtr psession, Json::Value const& msg)
    {
    }

    void HandleMsg_Stop(fe::SessionPtr psession, Json::Value const& msg)
    {
    }

    void HandleMsg_Query(fe::SessionPtr psession, Json::Value const& msg)
    {
    }

    asio::io_context ioc_;
    fe::FrontEnd fe_;
    fe::MessageDispatcher msg_dispatcher_;
};

#endif
