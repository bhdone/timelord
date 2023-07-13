#ifndef TL_CHALLENGE_MONITOR_HPP
#define TL_CHALLENGE_MONITOR_HPP

#include <boost/asio.hpp>
namespace asio = boost::asio;

#include <memory>
#include <set>
#include <tuple>

#include <string>
#include <string_view>

#include "common_types.h"

#include <rpc_client.h>

class ChallengeMonitor
{
public:
    enum class Status { NO_ERROR = 0, RPC_ERROR, OTHER_ERROR };

    using NewChallengeHandler = std::function<void(uint256 const& old_challenge, uint256 const& new_challenge, int height, uint64_t difficulty)>;

    using NewVdfReqHandler = std::function<void(uint256 const& challenge, std::set<uint64_t> const& vdf_reqs)>;

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

    std::set<uint64_t> const& GetVdfReqs() const
    {
        return vdf_reqs_;
    }

    void SetNewChallengeHandler(NewChallengeHandler handler);

    void SetNewVdfReqHandler(NewVdfReqHandler handler);

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
    std::set<uint64_t> vdf_reqs_;
    NewChallengeHandler new_challenge_handler_;
    NewVdfReqHandler new_vdf_req_handler_;
};

#endif
