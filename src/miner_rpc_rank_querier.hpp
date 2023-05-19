#ifndef MINER_RANK_QUERIER_HPP
#define MINER_RANK_QUERIER_HPP

#include <tuple>
#include <vector>

#include "rank_record.h"
#include "rpc_client.h"

class MinerRPCRankQuerier
{
public:
    MinerRPCRankQuerier(RPCClient& rpc, int from_height)
        : rpc_(rpc)
        , from_height_(from_height)
    {
    }

    std::tuple<std::vector<RankRecord>, int> operator()() const
    {
        std::vector<RankRecord> ranks;
        auto res = rpc_.Call("countblockowners", std::to_string(from_height_));
        auto keys = res.result.getKeys();
        for (auto const& key : keys) {
            if (key != "begin" && key != "end" && key != "count") {
                RankRecord rank;
                rank.address = key;
                rank.produced_blocks = res.result[key].get_int64();
                ranks.push_back(std::move(rank));
            }
        }
        return std::make_tuple(ranks, from_height_);
    }

private:
    RPCClient& rpc_;
    int from_height_;
};

#endif
