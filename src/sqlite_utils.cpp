#include "sqlite_utils.h"

#include <stdexcept>

#include <tinyformat.h>

void CheckSQL(sqlite3* sql3, int res)
{
    if (res != SQLITE_OK) {
        // get error string
        char const* errmsg = sqlite3_errmsg(sql3);
        throw std::runtime_error(tinyformat::format("SQLite error: %s", errmsg));
    }
}
