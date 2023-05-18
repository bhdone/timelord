#ifndef QUERIER_DEFS_H
#define QUERIER_DEFS_H

#include <functional>
#include <vector>

#include "block_info.h"
#include "netspace_data.h"
#include "rank_record.h"
#include "timelord_status.h"
#include "vdf_record.h"

using TimelordStatusQuerierType = std::function<TimelordStatus()>;

using LastBlockInfoQuerierType = std::function<BlockInfo()>;

using BlockInfoRangeQuerierType = std::function<std::vector<BlockInfo>(int num_heights)>;

using NumHeightsByHoursQuerierType = std::function<int(int hours)>;

using VDFPackByChallengeQuerierType = std::function<VDFRecordPack(uint256 const& challenge)>;

using NetspaceQuerierType = std::function<std::vector<NetspaceData>(int num_heights)>;

using RankQuerierType = std::function<RankRecord()>;

#endif
