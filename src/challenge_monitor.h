#ifndef TL_CHALLENGE_MONITOR_HPP
#define TL_CHALLENGE_MONITOR_HPP

#include <asio.hpp>

#include <tuple>

#include <string>
#include <string_view>

#include <plog/Log.h>

#include "common_types.h"
#include "timelord_utils.h"

#include <rpc_client.h>

class ChallengeMonitor
{
public:
    using NewChallengeHandler = std::function<void(uint256 const& old_challenge, uint256 const& new_challenge)>;

    ChallengeMonitor(asio::io_context& ioc, std::string_view url, std::string_view cookie_path, int interval_seconds);

    uint256 const& GetCurrentChallenge() const;

    void SetNewChallengeHandler(NewChallengeHandler handler);

    void Run();

    void Exit();

private:
    void QueryChallenge();

    void DoQueryNext();

    asio::io_context& ioc_;
    asio::steady_timer timer_;
    RPCClient rpc_;
    int interval_seconds_;
    uint256 challenge_;
    NewChallengeHandler new_challenge_handler_;
};

#endif
