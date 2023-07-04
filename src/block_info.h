#ifndef BLOCK_INFO_H
#define BLOCK_INFO_H

#include <cstdint>

#include <functional>

#include <string>

#include "common_types.h"

struct BlockInfo {
    uint256 hash;
    uint32_t timestamp;
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
    int vdf_iters_req;
};

using BlockInfoRangeQuerierType = std::function<std::vector<BlockInfo>(int)>;

using BlockInfoSaverType = std::function<void(BlockInfo const& block_info)>;

#endif
