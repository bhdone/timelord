#include "standard_status_querier.h"

#include "timelord.h"

StandardStatusQuerier::StandardStatusQuerier(LastBlockInfoQuerierType last_block_querier, VDFPackByChallengeQuerierType vdf_pack_querier, Timelord const& timelord)
    : last_block_querier_(std::move(last_block_querier))
    , vdf_pack_querier_(std::move(vdf_pack_querier))
    , timelord_(timelord)
{
}

TimelordStatus StandardStatusQuerier::operator()() const
{
    TimelordStatus status;
    auto timelord_status = timelord_.QueryStatus();
    status.challenge = timelord_status.challenge;
    status.height = timelord_status.height;
    status.iters_per_sec = timelord_status.iters_per_sec;
    status.total_size = timelord_status.total_size;
    status.num_connections = timelord_status.num_connections;

    status.last_block_info = last_block_querier_();
    status.vdf_pack = vdf_pack_querier_(status.challenge);
    return status;
}
