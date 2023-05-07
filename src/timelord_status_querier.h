#ifndef STATUS_QUERIER_H
#define STATUS_QUERIER_H

#include "vdf_record.h"
#include "block_querier.h"
#include "common_types.h"

struct TimelordStatus {
    uint256 challenge;
    int height;
    int iters_per_sec;
    uint64_t total_size;
    BlockQuerier::BlockInfo last_block_info;
    VDFRecordPack vdf_pack;
};

using TimelordStatusQuerier = std::function<TimelordStatus()>;

#endif
