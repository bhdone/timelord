#include "vdf_record.h"

bool operator==(VDFRecord const& lhs, VDFRecord const& rhs)
{
    return lhs.vdf_id == rhs.vdf_id && lhs.timestamp == rhs.timestamp && lhs.challenge == rhs.challenge && lhs.height == rhs.height && lhs.calculated == rhs.calculated;
}

bool operator!=(VDFRecord const& lhs, VDFRecord const& rhs)
{
    return !(lhs == rhs);
}

bool operator==(VDFRequest const& lhs, VDFRequest const& rhs)
{
    return lhs.vdf_id == rhs.vdf_id && lhs.iters == rhs.iters && lhs.estimated_seconds == rhs.estimated_seconds && lhs.group_hash == rhs.group_hash && lhs.netspace == rhs.netspace;
}

bool operator!=(VDFRequest const& lhs, VDFRequest const& rhs)
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
