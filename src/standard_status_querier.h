#ifndef STANDARD_STATUS_QUERIER_H
#define STANDARD_STATUS_QUERIER_H

#include "querier_defs.h"

#include "timelord_status.h"

class BlockQuerier;
class Timelord;

class LocalSQLiteStorage;
template <typename Storage> class LocalDatabaseKeeper;
using LocalSQLiteDatabaseKeeper = LocalDatabaseKeeper<LocalSQLiteStorage>;

class StandardStatusQuerier
{
public:
    StandardStatusQuerier(LastBlockInfoQuerierType last_block_querier, VDFPackByChallengeQuerierType vdf_pack_querier, Timelord const& timelord);

    TimelordStatus operator()() const;

private:
    LastBlockInfoQuerierType last_block_querier_;
    VDFPackByChallengeQuerierType vdf_pack_querier_;
    Timelord const& timelord_;
};

#endif
