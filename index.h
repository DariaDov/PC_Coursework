#include <map>
#include <mutex>
#include <iostream>
#include <vector>

struct DocIndex {
    std::vector<int> values;
    std::mutex mutex;
};

typedef std::map<std::string, DocIndex> IndexMap;

class safe_map {
private:
    IndexMap data;
    std::mutex mutex;
public:
    void insert(const std::string& key, const int value) {
        mutex.lock();
        auto [it, inserted] = data.try_emplace(key);
        DocIndex& docs = it->second;
        mutex.unlock();

        docs.mutex.lock();
        docs.values.push_back(value);
        docs.mutex.unlock();
    }

    std::vector<int> get(const std::string& key) {
        mutex.lock();
        auto it = data.find(key);
        if (it != data.end()) {
            mutex.unlock();
            return it->second.values;
        }
        mutex.unlock();
        return std::vector<int>();
    }
};