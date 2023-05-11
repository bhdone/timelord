#ifndef BLOCK_INFO_RANGE_SQLITE_QUERIER_HPP
#define BLOCK_INFO_RANGE_SQLITE_QUERIER_HPP

#include <vector>

#include "local_sqlite_storage.h"

#include "block_info.h"

class BlockInfoRangeLocalDBQuerier
{
public:
    explicit BlockInfoRangeLocalDBQuerier(LocalSQLiteStorage& storage)
        : storage_(storage)
    {
    }

    std::vector<BlockInfo> operator()(int num_heights) const
    {
        return storage_.QueryBlocksRange(num_heights);
    }

private:
    LocalSQLiteStorage& storage_;
};

#endif
