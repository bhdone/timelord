#include "status_querier.h"

StatusQuerier::StatusQuerier(BlockQuerier const& block_querier, ChallengeMonitor const& challenge_monitor, Timelord const& timelord, VDFSQLitePersistOperator const& persist_operator)
    : block_querier_(block_querier)
    , challenge_monitor_(challenge_monitor)
    , timelord_(timelord)
    , persist_operator_(persist_operator)
{
}

StatusQuerier::Status StatusQuerier::QueryCurrStatus() const
{
    Status status;
    status.challenge = challenge_monitor_.GetCurrentChallenge();
    auto timelord_status = timelord_.QueryStatus();
    status.height = timelord_status.height;
    status.iters_per_sec = timelord_status.iters_per_sec;
    status.total_size = timelord_status.total_size;
    // TODO: complete these status

    return status;
}
