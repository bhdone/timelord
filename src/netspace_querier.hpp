#ifndef NETSPACE_QUERIER_HPP
#define NETSPACE_QUERIER_HPP

#include <vector>

#include "netspace_data.h"

#include "local_sqlite_storage.h"

class NetspaceSQLiteQuerier
{
public:
    NetspaceSQLiteQuerier(LocalSQLiteStorage& storage, bool sum_netspace)
        : storage_(storage)
        , sum_netspace_(sum_netspace)
    {
    }

    std::vector<NetspaceData> operator()(int num_heights) const
    {
        return storage_.QueryNetspace(num_heights, sum_netspace_);
    }

private:
    LocalSQLiteStorage& storage_;
    bool sum_netspace_;
};

#endif
