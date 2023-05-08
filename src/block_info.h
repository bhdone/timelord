#ifndef BLOCK_INFO_H
#define BLOCK_INFO_H

#include <cstdint>

#include <string>

#include "common_types.h"

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

#endif
