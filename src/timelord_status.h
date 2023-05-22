#ifndef TIMELORD_STATUS_H
#define TIMELORD_STATUS_H

#include <cstdint>

#include "common_types.h"

#include "block_info.h"
#include "vdf_record.h"

struct TimelordStatus {
    std::string hostip;
    uint256 challenge;
    uint64_t difficulty;
    int height;
    int iters_per_sec;
    uint64_t total_size;
    uint64_t max_size;
    int num_connections;
    std::string status_string;
    BlockInfo last_block_info;
    VDFRecordPack vdf_pack;
};

#endif
