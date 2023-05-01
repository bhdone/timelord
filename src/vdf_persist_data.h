#ifndef VDF_PERSIST_DATA_H
#define VDF_PERSIST_DATA_H

#include <cstdint>
#include <ctime>

#include <string_view>
#include <vector>

#include "vdf_record.h"

template <typename Storage> class VDFPersistOperator
{
public:
    explicit VDFPersistOperator(Storage& storage)
        : storage_(storage)
    {
    }

    int64_t Save(VDFRecordPack const& pack)
    {
        return storage_.Save(pack);
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
            pack.requests = storage_.QueryRequests(record.vdf_id);
            res.push_back(std::move(pack));
        }
        return res;
    }

    std::tuple<VDFRecordPack, bool> QueryLastRecordPack() const
    {
        bool exists;
        VDFRecordPack pack;
        std::tie(pack.record, exists) = storage_.QueryLastRecord();
        if (!exists) {
            return std::make_tuple(pack, false);
        }
        pack.requests = storage_.QueryRequests(pack.record.vdf_id);
        return std::make_tuple(pack, true);
    }

private:
    Storage& storage_;
};

#endif
