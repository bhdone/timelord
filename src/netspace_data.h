#ifndef NETSPACE_DATA_HPP
#define NETSPACE_DATA_HPP

#include <cstdint>

struct NetspaceData {
    int height;
    uint64_t challenge_difficulty;
    uint64_t block_difficulty;
    uint64_t netspace;
};

#endif
