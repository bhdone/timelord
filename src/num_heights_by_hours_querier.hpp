#ifndef NUM_HEIGHTS_BY_HOURS_QUERIER_HPP
#define NUM_HEIGHTS_BY_HOURS_QUERIER_HPP

#include <cstdint>
#include <cstdlib>

#include "local_sqlite_storage.h"

class NumHeightsByHoursQuerier
{
public:
    explicit NumHeightsByHoursQuerier(LocalSQLiteStorage& storage)
        : storage_(storage)
    {
    }

    int operator()(int hours) const
    {
        return storage_.QueryNumHeightsByTimeRange(hours);
    }

private:
    LocalSQLiteStorage& storage_;
};

#endif
