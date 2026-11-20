#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include "data_struct.h"
#include "thread_pool.h"

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
using KeyValueMap = std::map<std::string, std::string>; 
using StringSet = std::set<std::string>;

DataArray books;