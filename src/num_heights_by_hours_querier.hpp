#ifndef NUM_HEIGHTS_BY_HOURS_QUERIER_HPP
#define NUM_HEIGHTS_BY_HOURS_QUERIER_HPP

#include <cstdint>
#include <cstdlib>

#include "local_sqlite_storage.h"

class NumHeightsByHoursQuerier
{
public:
    NumHeightsByHoursQuerier(LocalSQLiteStorage& storage, int fork_height)
        : storage_(storage)
        , fork_height_(fork_height)
    {
    }

    int operator()(int pass_hours) const
    {
        return storage_.QueryNumHeightsByTimeRange(pass_hours, fork_height_);
    }

private:
    LocalSQLiteStorage& storage_;
    int fork_height_;
};

#endif
