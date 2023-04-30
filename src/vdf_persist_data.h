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
        return storage_.Query(beg, curr);
    }

    VDFRecordPack QueryTheLatestRecord() const
    {
        return storage_.QueryLast();
    }

private:
    Storage& storage_;
};

#endif
