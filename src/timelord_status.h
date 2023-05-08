#ifndef TIMELORD_STATUS_H
#define TIMELORD_STATUS_H

#include <cstdint>

#include "common_types.h"

#include "block_info.h"
#include "vdf_record.h"

struct TimelordStatus {
    uint256 challenge;
    int height;
    int iters_per_sec;
    uint64_t total_size;
    BlockInfo last_block_info;
    VDFRecordPack vdf_pack;
};

#endif
