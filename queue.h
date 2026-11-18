#pragma once

#include <queue>
#include <mutex>
#include <shared_mutex>

template <typename T>
class MyQueue {
public:
    MyQueue() = default;
    MyQueue(const MyQueue<T>& src): task_queue(src.task_queue) {}
    ~MyQueue() { clear(); }

    bool empty() const {
        std::shared_lock lock(mtx);
        return task_queue.empty();
    }

    int size() const {
        std::shared_lock lock(mtx);
        return task_queue.size();
    }

    void clear() {
        std::unique_lock lock(mtx);
        
        while (!task_queue.empty()) {
            task_queue.pop();
        }
    }

    bool pop(T& task) {
        std::unique_lock lock(mtx);
        if (task_queue.empty()) {
            return false;
        }

        task = std::move(task_queue.front());
        task_queue.pop();
        return true;
    }


    T front() const {
        std::shared_lock lock(mtx);
        return task_queue.front();
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        std::unique_lock lock(mtx);
        task_queue.emplace(std::forward<Args>(args)...);
    }
private:
    std::queue<T> task_queue;
    mutable std::shared_mutex mtx;
};