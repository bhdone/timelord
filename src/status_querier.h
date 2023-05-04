#ifndef STATUS_QUERIER_H
#define STATUS_QUERIER_H

#include "vdf_record.h"

#include "block_querier.h"
#include "challenge_monitor.h"
#include "local_sqlite_storage.h"
#include "timelord.h"

class StatusQuerier
{
public:
    struct Status {
        uint256 challenge;
        int height;
        int iters_per_sec;
        uint64_t total_size;
        BlockQuerier::BlockInfo last_block_info;
        VDFRecordPack vdf_pack;
    };

    StatusQuerier(BlockQuerier const& block_querier, ChallengeMonitor const& challenge_monitor, Timelord const& timelord, VDFSQLitePersistOperator const& persist_operator);

    Status QueryCurrStatus() const;

private:
    BlockQuerier const& block_querier_;
    ChallengeMonitor const& challenge_monitor_;
    Timelord const& timelord_;
    VDFSQLitePersistOperator const& persist_operator_;
};

#endif
