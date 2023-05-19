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

    std::tuple<std::vector<RankRecord>, int> operator()() const {
        auto ranks = db_.QueryRank(start_height_, count_);
        return std::make_tuple(ranks, start_height_);
    }

private:
    LocalSQLiteStorage& db_;
    int start_height_;
    int count_;
};

#endif
