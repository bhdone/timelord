#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include <memory>
#include <string_view>

#include "vdf_record.h"

struct sqlite3;

class SQLiteStorage
{
public:
    explicit SQLiteStorage(std::string_view file_path);

    ~SQLiteStorage();

    void Save(VDFRecordPack const& pack);

    std::vector<VDFRecordPack> Query(uint32_t begin_timestamp, uint32_t end_timestamp) const;

    VDFRecordPack QueryLast() const;

private:
    std::unique_ptr<sqlite3> sql3_;
};

#endif
