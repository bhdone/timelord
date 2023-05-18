#ifndef RANK_RECORD_H
#define RANK_RECORD_H

#include <map>
#include <string>

struct RankRecord {
    int begin_height;
    int end_height;
    int count;
    std::map<std::string, int> entries;
};

#endif
