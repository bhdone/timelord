#ifndef VDF_RECORD_H
#define VDF_RECORD_H

#include <cstdint>

#include "common_types.h"

struct VDFRecord {
    uint64_t vdf_id;
    uint32_t timestamp;
    uint256 challenge;
    uint32_t height;
    bool calculated;
};

struct VDFRequest {
    uint64_t vdf_id;
    uint64_t iters;
    uint32_t estimated_seconds;
    uint256 group_hash;
    uint64_t netspace;
};

struct VDFRecordPack {
    VDFRecord record;
    std::vector<VDFRequest> requests;
};

#endif
