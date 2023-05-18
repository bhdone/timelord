#ifndef MINER_RANK_QUERIER_HPP
#define MINER_RANK_QUERIER_HPP

#include "rank_record.h"
#include "rpc_client.h"

class MinerRankQuerier
{
public:
    MinerRankQuerier(RPCClient& rpc, int from_height)
        : rpc_(rpc)
        , from_height_(from_height)
    {
    }

    RankRecord operator()() const
    {
        auto res = rpc_.Call("countblockowners", std::to_string(from_height_));
        RankRecord rank;
        auto keys = res.result.getKeys();
        for (auto const& key : keys) {
            if (key == "begin") {
                rank.begin_height = res.result["begin"].get_int64();
            } else if (key == "end") {
                rank.end_height = res.result["end"].get_int64();
            } else if (key == "count") {
                rank.count = res.result["count"].get_int64();
            } else {
                rank.entries.insert(std::make_pair(key, res.result[key].get_int64()));
            }
        }
        return rank;
    }

private:
    RPCClient& rpc_;
    int from_height_;
};

#endif
