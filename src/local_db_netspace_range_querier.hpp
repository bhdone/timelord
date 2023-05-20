#ifndef LOCAL_DB_NETSPACE_RANGE_QUERIER_HPP
#define LOCAL_DB_NETSPACE_RANGE_QUERIER_HPP

#include "local_sqlite_storage.h"

class LocalDBNetspaceRangeQuerier
{
public:
    LocalDBNetspaceRangeQuerier(LocalSQLiteStorage& db, int start_height)
        : db_(db)
        , start_height_(start_height)
    {
    }

    std::pair<uint64_t, uint64_t> operator()(int hours) const
    {
        if (hours != 0) {
            int num_heights = db_.QueryNumHeightsByTimeRange(hours);
            int curr_height = db_.QueryLastBlockHeight();
            int start_height = curr_height - num_heights;
            return db_.QueryNetspaceRange(start_height);
        } else {
            return db_.QueryNetspaceRange(start_height_);
        }
    }

private:
    LocalSQLiteStorage& db_;
    int start_height_;
};

#endif
