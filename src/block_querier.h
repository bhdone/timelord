#ifndef BLOCK_QUERIER_H
#define BLOCK_QUERIER_H

#include <cstdint>

#include <tuple>

#include <string>

#include <rpc_client.h>

#include "common_types.h"

class BlockQuerier
{
public:
    struct BlockInfo {
        uint256 challenge;
        int height;
        int filter_bits;
        uint64_t block_difficulty;
        uint64_t challenge_difficulty;
        Bytes farmer_pk;
        std::string address;
        double reward;
        uint64_t accumulate;
        std::string vdf_time;
        int vdf_iters;
        int vdf_speed;
    };

    explicit BlockQuerier(RPCClient& rpc);

    std::tuple<BlockInfo, bool> QueryBlockInfo(int height = -1, int max_heights_to_search = 3) const;

    std::tuple<BlockInfo, bool> QueryBlockInfo(uint256 const& challenge, int max_heights_to_search = 3) const;

private:
    RPCClient& rpc_;
};

#endif
