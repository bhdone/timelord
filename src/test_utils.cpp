#include "test_utils.h"

#include <cstring>

bool IsFlag(char const* sz_argv, char const* flag_name)
{
    int p { 0 };
    int n = strlen(sz_argv);
    if (n == 0 || sz_argv[0] != '-') {
        return false;
    }
    for (; p < n; ++p) {
        if (sz_argv[p] != '-') {
            break;
        }
    }
    return strcmp(sz_argv + p, flag_name) == 0;
}

void ParseCommandLineParams(int argc, char* argv[], bool& verbose)
{
    verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (IsFlag(argv[i], "verbose")) {
            verbose = true;
            break;
        }
    }
}

uint256 MakeRandomUInt256()
{
    uint256 res;
    for (int i = 0; i < 256 / 8; ++i) {
        res[i] = random() % 256;
    }
    return res;
}

Bytes MakeRandomBytes(std::size_t len)
{
    Bytes res(len, '\0');
    for (int i = 0; i < len; ++i) {
        res[i] = random() % 256;
    }
    return res;
}

VDFRecordPack GenerateRandomPack(uint32_t timestamp, uint32_t height, bool calculated)
{
    std::srand(time(nullptr));
    VDFRecordPack pack;
    pack.record.timestamp = timestamp;
    pack.record.challenge = MakeRandomUInt256();
    pack.record.height = height;
    return pack;
}

VDFRequest GenerateRandomRequest(uint256 challenge)
{
    VDFRequest request;
    request.challenge = std::move(challenge);
    request.iters = random();
    request.estimated_seconds = random();
    request.group_hash = MakeRandomUInt256();
    request.total_size = random();
    return request;
}
