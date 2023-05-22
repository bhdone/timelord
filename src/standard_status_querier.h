#ifndef STANDARD_STATUS_QUERIER_H
#define STANDARD_STATUS_QUERIER_H

#include "querier_defs.h"

#include "timelord_status.h"

class BlockQuerier;
class Timelord;

class StandardStatusQuerier
{
public:
    StandardStatusQuerier(LastBlockInfoQuerierType last_block_querier, VDFPackByChallengeQuerierType vdf_pack_querier, NetspaceSizeQuerierType netspace_max_querier, Timelord const& timelord, bool detect_hostip);

    TimelordStatus operator()() const;

private:
    LastBlockInfoQuerierType last_block_querier_;
    VDFPackByChallengeQuerierType vdf_pack_querier_;
    NetspaceSizeQuerierType netspace_max_querier_;
    Timelord const& timelord_;

    bool detect_hostip_;
    mutable std::string hostip_;
};

#endif
