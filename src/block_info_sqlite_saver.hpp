#ifndef BLOCK_INFO_SQLITE_SAVER_HPP
#define BLOCK_INFO_SQLITE_SAVER_HPP

#include "block_info.h"

#include "local_sqlite_storage.h"

class BlockInfoSQLiteSaver
{
public:
    explicit BlockInfoSQLiteSaver(LocalSQLiteStorage& storage)
        : storage_(storage)
    {
    }

    void operator()(BlockInfo const& block_info) const
    {
        storage_.AppendBlock(block_info);
    }

private:
    LocalSQLiteStorage& storage_;
};

#endif
