#include "standard_status_querier.h"

#include "timelord.h"

StandardStatusQuerier::StandardStatusQuerier(BlockQuerier const& block_querier, Timelord const& timelord, VDFSQLitePersistOperator const& persist_operator)
    : block_querier_(block_querier)
    , timelord_(timelord)
    , persist_operator_(persist_operator)
{
}

TimelordStatus StandardStatusQuerier::operator()() const
{
    TimelordStatus status;
    auto timelord_status = timelord_.QueryStatus();
    status.challenge = timelord_status.challenge;
    status.settled_challenge = timelord_status.settled_challenge;
    status.height = timelord_status.height;
    status.iters_per_sec = timelord_status.iters_per_sec;
    status.total_size = timelord_status.total_size;
    std::tie(status.last_block_info, std::ignore) = block_querier_.QueryBlockInfo(status.settled_challenge);
    std::tie(status.vdf_pack, std::ignore) = persist_operator_.QueryRecordPack(status.challenge);
    return status;
}
