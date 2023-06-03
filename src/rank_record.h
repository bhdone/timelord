#ifndef RANK_RECORD_H
#define RANK_RECORD_H

#include <vector>
#include <string>

struct RankRecord {
    std::string farmer_pk;
    int participated_blocks;
    int produced_blocks;
    uint64_t average_difficulty;
    uint64_t total_reward;
};

#endif
