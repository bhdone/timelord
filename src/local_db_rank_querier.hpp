#ifndef LOCAL_DB_RANK_QUERIER_HPP
#define LOCAL_DB_RANK_QUERIER_HPP

#include "local_sqlite_storage.h"
#include "rank_record.h"

class LocalDBRankQuerier
{
public:
    LocalDBRankQuerier(LocalSQLiteStorage& db, int start_height, int count)
        : db_(db)
        , start_height_(start_height)
        , count_(count)
    {
    }

    std::tuple<std::vector<RankRecord>, int> operator()(int hours) const
    {
        if (hours == 0) {
            auto ranks = db_.QueryRank(start_height_, count_);
            return std::make_tuple(ranks, start_height_);
        } else {
            int num_heights = db_.QueryNumHeightsByTimeRange(hours);
            int curr_height = db_.QueryLastBlockHeight();
            int start_height = num_heights == -1 ? start_height_ : curr_height - num_heights;
            auto ranks = db_.QueryRank(start_height, count_);
            return std::make_tuple(ranks, start_height);
        }
    }

private:
    LocalSQLiteStorage& db_;
    int start_height_;
    int count_;
};

#endif
