#ifndef NETSPACE_QUERIER_HPP
#define NETSPACE_QUERIER_HPP

#include <vector>

#include "netspace_data.h"

#include "local_sqlite_storage.h"

class NetspaceSQLiteQuerier
{
public:
    explicit NetspaceSQLiteQuerier(LocalSQLiteStorage& storage)
        : storage_(storage)
    {
    }

    std::vector<NetspaceData> operator()(int num_heights) const
    {
        return storage_.QueryNetspace(num_heights);
    }

private:
    LocalSQLiteStorage& storage_;
};

#endif
