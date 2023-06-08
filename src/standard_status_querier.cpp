#include "standard_status_querier.h"

#include "timelord.h"

#include "ip_addr_querier.hpp"

StandardStatusQuerier::StandardStatusQuerier(LastBlockInfoQuerierType last_block_querier, VDFPackByChallengeQuerierType vdf_pack_querier, NetspaceSizeQuerierType netspace_max_querier, Timelord const& timelord, bool detect_hostip)
    : last_block_querier_(std::move(last_block_querier))
    , vdf_pack_querier_(std::move(vdf_pack_querier))
    , netspace_max_querier_(std::move(netspace_max_querier))
    , timelord_(timelord)
    , detect_hostip_(detect_hostip)
{
}

TimelordStatus StandardStatusQuerier::operator()() const
{
    if (hostip_.empty() && detect_hostip_) {
        try {
            hostip_ = IpAddrQuerier::ToString(IpAddrQuerier()());
        } catch (std::exception const& e) {
            PLOGE << tinyformat::format("cannot retrieve server ip address, reason: %s", e.what());
        }
    }

    TimelordStatus status;
    try {
        auto timelord_status = timelord_.QueryStatus();
        status.hostip = hostip_.empty() ? "unavailable" : hostip_;
        status.challenge = timelord_status.challenge;
        status.difficulty = timelord_status.difficulty;
        status.height = timelord_status.height;
        status.iters_per_sec = timelord_status.iters_per_sec;
        status.total_size = timelord_status.total_size;
        status.max_size = netspace_max_querier_(0, status.height);
        status.status_string = timelord_status.status_string;
        status.num_connections = timelord_status.num_connections;
    } catch (std::exception const& e) {
        PLOGE << tinyformat::format("query status failed: %s", e.what());
    }

    try {
        status.last_block_info = last_block_querier_();
    } catch (std::exception const& e) {
        PLOGE << tinyformat::format("query last block info. failed: %s", e.what());
    }
    try {
        status.vdf_pack = vdf_pack_querier_(status.challenge);
    } catch (std::exception const& e) {
        PLOGE << tinyformat::format("query vdf info. failed: %s", e.what());
    }

    return status;
}
