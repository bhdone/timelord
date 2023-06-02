#ifndef PLEDGE_INFO_RPC_QUERIER_HPP
#define PLEDGE_INFO_RPC_QUERIER_HPP

#include "pledge_info.h"
#include "rpc_client.h"

class PledgeInfoRPCQuerier
{
public:
    explicit PledgeInfoRPCQuerier(RPCClient& rpc)
        : rpc_(rpc)
    {
    }

    PledgeInfo operator()() const
    {
        auto res = rpc_.Call("querypledgeinfo");
        if (!res.result.isObject()) {
            throw std::runtime_error("the type of the result from RPC command `querypledgeinfo` is not object");
        }
        PledgeInfo pledge_info;
        pledge_info.retarget_min_heights = res.result["retarget_min_heights"].get_int();
        pledge_info.capacity_eval_window = res.result["capacity_eval_window"].get_int();
        auto terms_json = res.result["terms"].getValues();
        int i { 0 };
        for (auto const& term_json : terms_json) {
            PledgeItem pledge;
            pledge.lock_height = term_json["lock_height"].get_int();
            pledge.actual_percent = term_json["actual_percent"].get_int();
            pledge_info.pledges[i++] = pledge;
        }
        return pledge_info;
    }

private:
    RPCClient& rpc_;
};

#endif
