#ifndef SUPPLY_DATA_H
#define SUPPLY_DATA_H

#include <cstdint>

struct SupplyOnHeight {
    int height;
    uint64_t burned;
    uint64_t total;
    uint64_t actual;
};

struct Supply {
    int dist_height;
    SupplyOnHeight calc;
    SupplyOnHeight last;
};

#endif
