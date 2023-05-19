#ifndef MINER_RANK_QUERIER_HPP
#define MINER_RANK_QUERIER_HPP

#include <tuple>
#include <vector>

#include "rank_record.h"
#include "rpc_client.h"

class MinerRPCRankQuerier
{
public:
    MinerRPCRankQuerier(RPCClient& rpc, int start_height, int count)
        : rpc_(rpc)
        , start_height_(start_height)
        , count_(count)
    {
    }

    std::tuple<std::vector<RankRecord>, int> operator()() const
    {
        std::vector<RankRecord> ranks;
        auto res = rpc_.Call("countblockowners", std::to_string(start_height_));
        auto keys = res.result.getKeys();
        int curr_count { 0 };
        for (auto const& key : keys) {
            if (key != "begin" && key != "end" && key != "count") {
                RankRecord rank;
                rank.address = key;
                rank.produced_blocks = res.result[key].get_int64();
                ranks.push_back(std::move(rank));
                ++curr_count;
                if (curr_count == count_) {
                    break;
                }
            }
        }
        return std::make_tuple(ranks, start_height_);
    }

private:
    RPCClient& rpc_;
    int start_height_;
    int count_;
};

#endif
