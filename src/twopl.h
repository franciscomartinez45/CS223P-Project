#pragma once
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <chrono>
#include <thread>
#include <random>
#include "database.h"
#include "transaction.h"

class TwoPL {
public:
    explicit TwoPL(Database& database) : db_(database) {}

    template<typename Callable>
    TransactionStats run(int transaction_id, Callable transaction_func);

    long long get_committed_count() const { return committed_.load(); }
    long long get_aborted_count()   const { return aborted_.load(); }

private:
    Database& db_;
    std::mutex map_mutex_;
    std::unordered_map<std::string, std::mutex> lock_table_;
    std::atomic<long long> committed_{0};
    std::atomic<long long> aborted_{0};

    bool try_acquire_all(const std::unordered_set<std::string>& keys,
                         std::unordered_set<std::string>& acquired_locks) {
        for (const auto& key : keys) {
            std::mutex* key_mutex = nullptr;
            {
                std::lock_guard<std::mutex> map_lock(map_mutex_);
                key_mutex = &lock_table_[key];
            }
            if (key_mutex->try_lock()) {
                acquired_locks.insert(key);
            } else {
                release_all(acquired_locks);
                return false;
            }
        }
        return true;
    }

    void release_all(std::unordered_set<std::string>& acquired_locks) {
        std::lock_guard<std::mutex> map_lock(map_mutex_);
        for (const auto& key : acquired_locks) {
            auto it = lock_table_.find(key);
            if (it != lock_table_.end())
                it->second.unlock();
        }
        acquired_locks.clear();
    }
};

template<typename Callable>
TransactionStats TwoPL::run(int transaction_id, Callable transaction_func) {
    TransactionStats stats;
    auto start_time = std::chrono::high_resolution_clock::now();

    std::unordered_set<std::string> keys_needed;
    std::unordered_set<std::string> acquired_locks;
    {
        Transaction probe(transaction_id, db_);
        transaction_func(probe);
        keys_needed = probe.get_all_keys();
    }

    thread_local std::mt19937 random_engine(std::random_device{}());

    while (true) {
        acquired_locks.clear();
        if (try_acquire_all(keys_needed, acquired_locks)) {
            Transaction transaction(transaction_id, db_);
            transaction_func(transaction);
            transaction.flush_writes();
            transaction.set_status(TransactionStatus::COMMITTED);
            release_all(acquired_locks);
            committed_++;

            auto end_time = std::chrono::high_resolution_clock::now();
            stats.response_time_ms =
                std::chrono::duration<double, std::milli>(end_time - start_time).count();
            return stats;
        }

        aborted_++;
        stats.retries++;
        std::uniform_int_distribution<int> backoff(100, 500);
        std::this_thread::sleep_for(std::chrono::microseconds(backoff(random_engine)));
    }
}
