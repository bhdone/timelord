#ifndef TL_CHALLENGE_MONITOR_HPP
#define TL_CHALLENGE_MONITOR_HPP

#include "asio_defs.hpp"

#include <memory>
#include <tuple>

#include <string>
#include <string_view>

#include "common_types.h"
#include "timelord_utils.h"

#include <rpc_client.h>

class ChallengeMonitor
{
public:
    enum class Status { NO_ERROR = 0, RPC_ERROR, OTHER_ERROR };

    using NewChallengeHandler = std::function<void(uint256 const& old_challenge, uint256 const& new_challenge, int height)>;

    ChallengeMonitor(asio::io_context& ioc, RPCClient& rpc, int interval_seconds);

    uint256 const& GetCurrentChallenge() const;

    Status GetStatus() const
    {
        return status_;
    }

    std::string GetErrorString() const
    {
        return error_string_;
    }

    void SetNewChallengeHandler(NewChallengeHandler handler);

    void Run();

    void Exit();

private:
    void QueryChallenge();

    void DoQueryNext();

    asio::io_context& ioc_;
    asio::steady_timer timer_;
    RPCClient& rpc_;
    Status status_ { Status::NO_ERROR };
    std::string error_string_;
    int interval_seconds_;
    uint256 challenge_;
    NewChallengeHandler new_challenge_handler_;
};

#endif
