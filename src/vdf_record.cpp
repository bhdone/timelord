#include "vdf_record.h"

bool operator==(VDFRecord const& lhs, VDFRecord const& rhs)
{
    return lhs.timestamp == rhs.timestamp && lhs.challenge == rhs.challenge && lhs.height == rhs.height;
}

bool operator!=(VDFRecord const& lhs, VDFRecord const& rhs)
{
    return !(lhs == rhs);
}

bool operator==(VDFRequest const& lhs, VDFRequest const& rhs)
{
    return lhs.iters == rhs.iters && lhs.estimated_seconds == rhs.estimated_seconds && lhs.group_hash == rhs.group_hash;
}

bool operator!=(VDFRequest const& lhs, VDFRequest const& rhs)
{
    return !(lhs == rhs);
}

bool operator==(VDFResult const& lhs, VDFResult const& rhs)
{
    return lhs.challenge == rhs.challenge && lhs.iters == rhs.iters && lhs.y == rhs.y && lhs.proof == rhs.proof && lhs.witness_type == rhs.witness_type;
}

bool operator!=(VDFResult const& lhs, VDFResult const& rhs)
{
    return !(lhs == rhs);
}

bool operator==(VDFRecordPack const& lhs, VDFRecordPack const& rhs)
{
    if (lhs.requests.size() != rhs.requests.size()) {
        return false;
    }
    if (lhs.record != rhs.record) {
        return false;
    }
    for (int i = 0; i < lhs.requests.size(); ++i) {
        if (lhs.requests[i] != rhs.requests[i]) {
            return false;
        }
    }
    return true;
}

bool operator!=(VDFRecordPack const& lhs, VDFRecordPack const& rhs)
{
    return !(lhs == rhs);
}
