#ifndef SQLITE_UTILS_H
#define SQLITE_UTILS_H

#include <string_view>

#include <sqlite3.h>

void CheckSQL(sqlite3* sql3, int res);

#endif
