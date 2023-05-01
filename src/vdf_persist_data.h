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

    void Save(VDFRecordPack const& pack)
    {
        storage_.Save(pack);
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

    VDFRecordPack QueryLastRecord() const
    {
        VDFRecordPack pack;
        pack.record = storage_.QueryLastRecord();
        pack.requests = storage_.QueryRequests(pack.record.vdf_id);
        return pack;
    }

private:
    Storage& storage_;
};

#endif
