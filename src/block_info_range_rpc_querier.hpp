#ifndef BLOCK_INFO_RANGE_QUERIER_HPP
#define BLOCK_INFO_RANGE_QUERIER_HPP

#include "common_types.h"

#include "rpc_client.h"

#include "block_querier_utils.h"

class BlockInfoRangeRPCQuerier
{
public:
    explicit BlockInfoRangeRPCQuerier(RPCClient& rpc)
        : rpc_(rpc)
    {
    }

    std::vector<BlockInfo> operator()(int num_heights) const
    {
        auto res = rpc_.Call("queryupdatetiphistory", std::to_string(num_heights));
        if (!res.result.isObject()) {
            throw std::runtime_error("the return value is not an array");
        }
        if (!res.result.exists("tips") || !res.result["tips"].isArray()) {
            throw std::runtime_error("the member `tips` doesn't exist");
        }
        std::vector<BlockInfo> blocks;
        auto values = res.result["tips"].getValues();
        blocks.reserve(values.size());
        for (auto const& block_json : values) {
            blocks.push_back(ConvertToBlockInfo(block_json));
        }
        return blocks;
    }

private:
    RPCClient& rpc_;
};

#endif
