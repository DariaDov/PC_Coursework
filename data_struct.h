#pragma once
#include <mutex>
#include <shared_mutex>
#include <set>

#define HASHMAP_SIZE_FACTOR 16 // 2^16=65536
#define THREADSAFEVECTOR_CAPACITY 10

template<typename T>
class ThreadSafeVector {
private:
    T* _data;
    std::size_t _size;
    std::size_t _capacity;
    mutable std::shared_mutex mtx;

    void expand() {
        _capacity = _capacity ? _capacity * 2 : THREADSAFEVECTOR_CAPACITY;
        T* new_data = new T[_capacity];
        for (std::size_t i = 0; i < _size; i++) {
            new_data[i] = std::move(_data[i]);
	    }
        std::swap(_data, new_data);
        delete[] new_data;
    }

public:
    ThreadSafeVector(std::size_t capacity) {
        _capacity = capacity;
        _size = 0;
        _data = new T[_capacity];
    }

    ThreadSafeVector(): ThreadSafeVector(THREADSAFEVECTOR_CAPACITY) {};
    
    ThreadSafeVector(const ThreadSafeVector& src):_size(src._size), _capacity(src._capacity) {
        _data = new T[_capacity];
        std::copy(src._data, src._data + src._size, _data);
    }

    ThreadSafeVector(ThreadSafeVector&& src): _data{src._data}, _size(src._size), _capacity(src._capacity) {
        src._size = 0;
        src._capacity = 0;
        src._data = nullptr;
    }

    ~ThreadSafeVector() {
        delete[] _data;
    }

    size_t push_back(const T& value) {
        std::unique_lock lock(mtx);
        if (_size == _capacity) {
            expand();
        }
        _data[_size++] = value;
        return _size;
    }

    size_t push_back(T&& value) {
        std::unique_lock lock(mtx);
        if (_size == _capacity) {
            expand();
        }
        _data[_size++] = std::move(value);
        return _size;
    }

    std::size_t size() const {
        std::shared_lock lock(mtx);
        return _size;
    }

    void clear() {
        std::unique_lock lock(mtx);
        _size = 0;
    }
        
    T& operator[](std::size_t idx) {
        std::shared_lock lock(mtx);
        return _data[idx];
    }

    const T& operator[](std::size_t idx) const {
        std::shared_lock lock(mtx);
	    return _data[idx];
    }

    T& back() const {
        std::shared_lock lock(mtx);
	    return _data[_size - 1];
    }

    ThreadSafeVector& operator=(ThreadSafeVector&& src) {
        std::unique_lock lock(mtx);
        if (this == &src) {
            return *this;
        }

        delete[] _data;
        _data = src._data;
        _size = src._size;
        _capacity = src._capacity;

        src._data = nullptr;
        src._size = 0;
        src._capacity = 0;

        return *this;
    }
};

template<typename K, typename V>
class HashMap {
private:
    using KeyValue = std::pair<K, V>;
    using KeyValueSet = ThreadSafeVector<KeyValue>;
    
    int factor;
    int last_element_index;
    KeyValueSet* data;
    mutable std::shared_mutex mtx;

    std::size_t index(const K& key) const {
	    return std::hash<K>{}(key) & (last_element_index);
    }

    bool no_key(std::size_t idx) const {
    	return data[idx].size() == 0;
    }

public:
    HashMap(): HashMap(HASHMAP_SIZE_FACTOR) {};

    HashMap(int factor) {
        this->factor = factor;
        last_element_index = (1 << factor) - 1;
        data = new KeyValueSet[last_element_index + 1];
    }

    ~HashMap() {
	    delete[](data);
    }

    bool contains(const K& key) const {
        std::shared_lock lock(mtx);
        auto idx = index(key);
        if (no_key(idx)) return false;
        auto& keys_values = data[idx];

        for (int i = 0; i < keys_values.size(); i++) {
            if (keys_values[i].first == key) return true;
        }
        return false;
    }

    void clear() {
        std::unique_lock lock(mtx);
        for (std::size_t i = 0; i <= last_element_index; i++) {
            data[i].clear();
        }
    }

    void print_stats() {
        std::shared_lock lock(mtx);
        int keys = 0;
        int cells = 0;
        for (int i = 0; i <= last_element_index; i++) {
            std::cout << i << ":" << data[i].size();
            for (int n = 0; n < data[i].size(); n++) {
                std::cout << ": " << data[i][n].first << " (" << data[i][n].second.size() << ") ";
            }
            if (data[i].size() > 0) {
                keys += data[i].size();
                cells++;
            }
            
            std::cout << std::endl;
        }

        std::cout << "avg: " << ((float) keys) / cells << std::endl;
        std::cout << "usage ratio: " << ((float) cells) / (last_element_index + 1) << std::endl;
    }

    void push_back(const std::set<K>& keys, auto val) {
        std::unique_lock lock(mtx);
        for (auto key: keys) {
            auto idx = index(key);
            bool found = false;
            for (int i = 0; i < data[idx].size(); i++) {
                if (data[idx][i].first == key) {
                    data[idx][i].second.push_back(val);
                    found = true;
                    break;
                }
            }

            if (!found) {
                data[idx].push_back(std::move(KeyValue{key, V()}));
                data[idx].back().second.push_back(val);
            }
        }
    }

    V at(const K& key) const {
        std::shared_lock lock(mtx);
        auto idx = index(key);
        if (no_key(idx)) return V();
        auto& keys_values = data[idx];

        for (int i = 0; i < keys_values.size(); i++) {
            if (keys_values[i].first == key) return keys_values[i].second;
        }
        return V();
    }
};

using StopWords = HashMap<std::string, int>;
using IndexMap = HashMap<std::string, ThreadSafeVector<std::size_t>>;

StopWords stop_words;

IndexMap tag_index(10);
IndexMap text_index(16);