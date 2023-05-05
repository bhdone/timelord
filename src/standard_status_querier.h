#ifndef STANDARD_STATUS_QUERIER_H
#define STANDARD_STATUS_QUERIER_H

#include "timelord_status_querier.h"

class BlockQuerier;
class Timelord;

class LocalSQLiteStorage;
template <typename Storage> class VDFPersistOperator;
using VDFSQLitePersistOperator = VDFPersistOperator<LocalSQLiteStorage>;

class StandardStatusQuerier
{
public:
    StandardStatusQuerier(BlockQuerier const& block_querier, Timelord const& timelord, VDFSQLitePersistOperator const& persist_operator);

    TimelordStatus operator()() const;

private:
    BlockQuerier const& block_querier_;
    Timelord const& timelord_;
    VDFSQLitePersistOperator const& persist_operator_;
};


#endif
