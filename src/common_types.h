#ifndef TL_COMMON_TYPES_H
#define TL_COMMON_TYPES_H

#include <cstdint>

#include <array>
#include <vector>
#include <string>

using Bytes = std::vector<uint8_t>;
using uint256 = std::array<uint8_t, 256 / 8>;
using VdfForm = std::array<uint8_t, 100>;

#endif
