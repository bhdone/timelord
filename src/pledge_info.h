#ifndef PLEDGE_INFO_H
#define PLEDGE_INFO_H

#include <array>

struct PledgeItem {
    int lock_height;
    int actual_percent;
};

struct PledgeInfo {
    int retarget_min_heights;
    int capacity_eval_window;
    std::array<PledgeItem, 4> pledges;
};

#endif
