#ifndef TL_CHALLENGE_MONITOR_HPP
#define TL_CHALLENGE_MONITOR_HPP

#include <asio.hpp>

#include <tuple>

#include <string>
#include <string_view>

#include <plog/Log.h>

#include "common_types.h"
#include "utils.h"

#include "rpc_client.h"

class ChallengeMonitor
{
public:
    using NewChallengeHandler = std::function<void(uint256 const&)>;

    ChallengeMonitor(asio::io_context& ioc, std::string url, std::string cookie_path, int interval_seconds, NewChallengeHandler handler)
            : ioc_(ioc), timer_(ioc), rpc_(true, url, cookie_path), interval_seconds_(interval_seconds), new_challenge_handler_(std::move(handler)) {}

    void Start()
    {
        timer_.expires_after(std::chrono::seconds(interval_seconds_));
        timer_.async_wait(
                [this](std::error_code const& ec) {
                    if (ec) {
                        if (ec != asio::error::operation_aborted) {
                            PLOGE << ec.message();
                        } else {
                            PLOGD << "timer aborted";
                        }
                        return;
                    }
                    // ready
                    QueryChallenge();
                    // next
                    Start();
                });
    }

    void Stop()
    {
        std::error_code ignored_ec;
        timer_.cancel(ignored_ec);
    }

    void QueryChallenge()
    {
        try {
            RPCClient::Result result = rpc_.Call("querychallenge");
            uint256 challenge = Uint256FromHex(result.result["challenge"].asString());
            if (challenge != challenge_) {
                challenge_ = challenge;
                if (new_challenge_handler_) {
                    new_challenge_handler_(challenge);
                }
            }
        } catch (std::exception const& e) {
            PLOGE << e.what();
        }
    }

private:
    asio::io_context& ioc_;
    asio::steady_timer timer_;
    RPCClient rpc_;
    int interval_seconds_;
    uint256 challenge_;
    NewChallengeHandler new_challenge_handler_;
};

#endif
