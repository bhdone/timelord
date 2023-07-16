#ifndef LAST_BLOCK_INFO_QUERIER_HPP
#define LAST_BLOCK_INFO_QUERIER_HPP

#include "block_info.h"
#include "block_querier_utils.h"
#include "rpc_client.h"

class LastBlockInfoQuerier
{
public:
    explicit LastBlockInfoQuerier(RPCClient& rpc)
        : rpc_(rpc)
    {
    }

    BlockInfo operator()() const
    {
        BlockInfo block_info;

        auto res = rpc_.Call("queryupdatetiphistory", std::string("1"));
        if (!res.result.isObject()) {
            throw std::runtime_error("the return value is not an object");
        }

        if (!res.result.exists("tips") || !res.result["tips"].isArray()) {
            throw std::runtime_error("`tips` doesn't exist or it is not an array");
        }

        auto values = res.result["tips"].getValues();
        if (values.empty()) {
            throw std::runtime_error("empty result from `queryupdatetiphistory`");
        }

        return ConvertToBlockInfo(values.front());
    }

private:
    RPCClient& rpc_;
};

#endif
