#pragma once

#include <string>
#include <vector>
#include "data_struct.h"

struct Data {
    std::string title;
    std::string author;
    std::string year;
    std::string page_count;
    std::string text;
};

struct IndexStatistic {
    int documents;
    unsigned int index_build_time_ms;
    short threads_used;
} index_stat;

using DataArray = ThreadSafeVector<Data>;

DataArray books;