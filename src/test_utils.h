#ifndef TL_TEST_UTILS_H
#define TL_TEST_UTILS_H

#include "timelord_utils.h"

#include "vdf_record.h"

bool IsFlag(char const* sz_argv, char const* flag_name);

void ParseCommandLineParams(int argc, char* argv[], bool& verbose);

VDFRecordPack GenerateRandomPack(uint32_t timestamp, uint32_t height, bool calculated);

VDFRequest GenerateRandomRequest(uint256 challenge);

VDFResult GenerateRandomResult(uint256 challenge, uint64_t iters, int dur);

#endif
