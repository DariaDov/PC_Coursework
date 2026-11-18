#pragma once

#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>
#include <condition_variable>

#include "queue.h"

// std::shared_lock lock(mtx);
// std::unique_lock lock(mtx);

template<typename T, auto W>
class ThreadPool {
public:
    ThreadPool() = default;
    ThreadPool(const ThreadPool<T, W>& src) {} 
    ~ThreadPool() { terminate(); }

    std::atomic<int> tasks_complited{0};
    std::atomic<int> queue_size{0};

    void initialize(int pool_size) {
        std::unique_lock lock(mtx);

        if (initialized || terminated) return;

        workers.reserve(pool_size);
        for (int i = 0; i < pool_size; ++i) {
            workers.emplace_back(&ThreadPool::routine, this);
        }

        initialized = !workers.empty();

        tasks_complited = 0;
        queue_size = 0;
    }

    void enqueue(T task) {
        if (!working()) {
            return;
        }

        tasks.emplace(task);
        condition.notify_one();
    }

    void terminate() {
        terminate(terminated);
    }

    void force_terminate() {
        terminate(force_terminated);
    }

    void pause() {
        std::unique_lock lock(mtx);
        paused = true;
    }

    void resume() {
        {
            std::unique_lock lock(mtx);
            paused = false;
        }
        condition.notify_all();
    }

private:
    mutable std::shared_mutex mtx;
    mutable std::condition_variable_any condition;
    std::vector<std::thread> workers;
    MyQueue<T> tasks;
    std::atomic<int> worker_id{0};

    bool initialized = false;
    bool force_terminated = false;
    bool terminated = false;
    bool paused = false;

    void routine() {
        int id = worker_id.fetch_add(1);
        while (true) {
            if (terminated || force_terminated) {
                return; 
            }

            T task;
            {
                std::unique_lock lock(mtx);
                auto wait_condition = [this] {
                    return !terminated && !force_terminated && !tasks.empty() && !paused;
                };

                condition.wait(lock, wait_condition);

                if (!tasks.empty()) {
                    queue_size.fetch_add(tasks.size());
                    tasks.pop(task);
                }
            }
            
            // some function
            W(task);

            // if (task.callback) {
            //     task.callback(task);
            // }
        }
    }

    bool working() {
        std::shared_lock lock(mtx);
        return working_unsafe();
    }

    bool working_unsafe() const {
        return initialized && !terminated && !force_terminated;
    }

    void reset() {
        std::unique_lock lock(mtx);
        workers.clear();
        terminated = false;
        force_terminated = false;
        paused = false;
        initialized = false;
        worker_id = 0;
    }

    void terminate(bool& termination_f) {
        {
            std::unique_lock lock(mtx);
            
            if (working_unsafe()) {
                termination_f = true;
            }
            else {
                return;
            }
        }

        condition.notify_all();
        
        for (std::thread& worker : workers) {
            if (worker.joinable()){
                worker.join();
            }
        }

        reset();
    }
};