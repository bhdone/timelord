#ifndef VDF_RECORD_H
#define VDF_RECORD_H

#include <cstdint>

#include "common_types.h"

struct VDFRecord {
    uint32_t timestamp;
    uint256 challenge;
    uint32_t height;
};

struct VDFRequest {
    uint256 challenge;
    uint64_t iters;
    uint32_t estimated_seconds;
    uint256 group_hash;
    uint64_t total_size;
};

struct VDFResult {
    uint256 challenge;
    uint64_t iters;
    Bytes y;
    Bytes proof;
    uint8_t witness_type;
    int duration;
};

struct VDFRecordPack {
    VDFRecord record;
    std::vector<VDFRequest> requests;
};

bool operator==(VDFRecord const& lhs, VDFRecord const& rhs);
bool operator!=(VDFRecord const& lhs, VDFRecord const& rhs);

bool operator==(VDFRequest const& lhs, VDFRequest const& rhs);
bool operator!=(VDFRequest const& lhs, VDFRequest const& rhs);

bool operator==(VDFResult const& lhs, VDFResult const& rhs);
bool operator!=(VDFResult const& lhs, VDFResult const& rhs);

bool operator==(VDFRecordPack const& lhs, VDFRecordPack const& rhs);
bool operator!=(VDFRecordPack const& lhs, VDFRecordPack const& rhs);

#endif
