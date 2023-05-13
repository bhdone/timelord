#ifndef VDF_PACK_BY_CHALLENGE_QUERIER_HPP
#define VDF_PACK_BY_CHALLENGE_QUERIER_HPP

#include <stdexcept>

#include "common_types.h"

#include "vdf_record.h"

#include "local_sqlite_storage.h"

class VDFPackByChallengeQuerier
{
public:
    explicit VDFPackByChallengeQuerier(LocalSQLiteStorage& storage)
        : storage_(storage)
    {
    }

    VDFRecordPack operator()(uint256 const& challenge) const
    {
        bool exists;
        VDFRecordPack pack;
        std::tie(pack.record, exists) = storage_.QueryRecord(challenge);
        if (!exists) {
            throw std::runtime_error("cannot find the related record");
        }
        pack.requests = storage_.QueryRequests(pack.record.challenge);
        return pack;
    }

private:
    LocalSQLiteStorage& storage_;
};

#endif
