#ifndef LOCAL_DB_RANK_QUERIER_HPP
#define LOCAL_DB_RANK_QUERIER_HPP

#include "local_sqlite_storage.h"
#include "rank_record.h"

class LocalDBRankQuerier
{
public:
    LocalDBRankQuerier(LocalSQLiteStorage& db, int fork_height, int count)
        : db_(db)
        , fork_height_(fork_height)
        , count_(count)
    {
    }

    std::tuple<std::vector<RankRecord>, int> operator()(int pass_hours) const
    {
        if (pass_hours == 0) {
            auto ranks = db_.QueryRank(fork_height_, count_);
            return std::make_tuple(ranks, fork_height_);
        } else {
            int num_heights = db_.QueryNumHeightsByTimeRange(pass_hours, fork_height_);
            int curr_height = db_.QueryLastBlockHeight();
            int fork_height = num_heights == -1 ? fork_height_ : curr_height - num_heights;
            auto ranks = db_.QueryRank(fork_height, count_);
            return std::make_tuple(ranks, fork_height);
        }
    }

private:
    LocalSQLiteStorage& db_;
    int fork_height_;
    int count_;
};

#endif
