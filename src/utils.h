#ifndef TL_UTILS_H
#define TL_UTILS_H

#include <string>

#include "common_types.h"

VdfForm MakeZeroForm();

std::string BytesToHex(Bytes const& bytes);
Bytes BytesFromHex(std::string const& hex);
Bytes MakeBytes(uint256 const& source);
std::string GetHex(uint256 const& source);
std::string FormatNumberStr(std::string const& str);


#endif
