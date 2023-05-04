#ifndef VDF_PERSIST_DATA_H
#define VDF_PERSIST_DATA_H

#include <cstdint>
#include <ctime>

#include <string_view>

#include <tuple>
#include <vector>

#include "vdf_record.h"

template <typename Storage> class VDFPersistOperator
{
public:
    explicit VDFPersistOperator(Storage& storage)
        : storage_(storage)
    {
    }

    void Save(VDFRecordPack const& pack)
    {
        storage_.Save(pack);
    }

    void AppendRecord(VDFRecord const& record)
    {
        storage_.AppendRecord(record);
    }

    void AppendRequest(VDFRequest const& request)
    {
        storage_.AppendRequest(request);
    }

    void AppendResult(VDFResult const& result)
    {
        storage_.AppendResult(result);
    }

    std::vector<VDFRecordPack> QueryRecordsInHours(uint32_t hours) const
    {
        uint32_t curr = time(nullptr);
        uint32_t seconds = hours * 60 * 60;
        uint32_t beg = curr - seconds;
        std::vector<VDFRecordPack> res;
        std::vector<VDFRecord> records = storage_.QueryRecords(beg, curr);
        for (auto const& record : records) {
            VDFRecordPack pack;
            pack.record = record;
            pack.requests = storage_.QueryRequests(record.challenge);
            pack.results = storage_.QueryResults(record.challenge);
            res.push_back(std::move(pack));
        }
        return res;
    }

    std::tuple<VDFRecordPack, bool> QueryRecordPack(uint256 const& challenge) const
    {
        bool exists;
        VDFRecordPack pack;
        std::tie(pack.record, exists) = storage_.QueryRecord(challenge);
        if (!exists) {
            return std::make_tuple(pack, false);
        }
        pack.requests = storage_.QueryRequests(pack.record.challenge);
        pack.results = storage_.QueryResults(pack.record.challenge);
        return std::make_tuple(pack, true);
    }

private:
    Storage& storage_;
};

#endif
