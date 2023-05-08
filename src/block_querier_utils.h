#ifndef BLOCK_QUERIER_UTILS_H
#define BLOCK_QUERIER_UTILS_H

#include <univalue.h>

#include "block_info.h"

BlockInfo ConvertToBlockInfo(UniValue const& block_json);

#endif
