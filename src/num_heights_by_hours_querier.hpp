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
        uint32_t end_timestamp = std::time(nullptr);
        uint32_t begin_timestamp = end_timestamp - hours * 60 * 60;
        return storage_.QueryNumHeightsByTimeRange(begin_timestamp, end_timestamp);
    }

private:
    LocalSQLiteStorage& storage_;
};

#endif
