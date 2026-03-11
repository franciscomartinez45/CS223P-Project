#include "transaction.h"
#pragma once
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <chrono>
#include <thread>
#include "database.h"
#include "transaction.h"

class TwoPL {
public:
    explicit TwoPL(Database& db);

    // Run a transaction under Conservative 2PL.
    // func must declare all keys it needs via txn.declare_keys(...)
    // before doing any reads/writes.
    template<typename TxnFunc>
    TxnStats run(int txn_id, TxnFunc func);

    long long get_committed_count() const { return committed_.load(); }
    long long get_aborted_count()   const { return aborted_.load(); }

private:
    Database& db_;

    // Per-key mutex. Access protected by map_mutex_.
    std::mutex map_mutex_;
    std::unordered_map<std::string, std::mutex> lock_table_;

    std::atomic<long long> committed_{0};
    std::atomic<long long> aborted_{0};

    // Try to acquire all locks for the given keys.
    // Returns true if successful, false if any lock was unavailable.
    // If false, all acquired locks are released before returning.
    bool try_acquire_all(const std::unordered_set<std::string>& keys,
                         std::unordered_set<std::string>& acquired);

    void release_all(std::unordered_set<std::string>& acquired);
};

template<typename TxnFunc>
TxnStats TwoPL::run(int txn_id, TxnFunc func) {
    TxnStats stats;
    auto wall_start = std::chrono::high_resolution_clock::now();

    // First, determine all keys this transaction will access.
    // We do a dry-run with a special transaction to collect keys,
    // then acquire locks, then execute for real.
    std::unordered_set<std::string> keys_needed;
    std::unordered_set<std::string> acquired_locks;

    // Execute func once to discover keys (no DB writes happen since
    // writes go to buffer and we don't flush)
    {
        Transaction probe(txn_id, db_);
        func(probe);
        keys_needed = probe.get_all_keys();
    }

    // Keep retrying until we get all locks
    while (true) {
        acquired_locks.clear();

        if (try_acquire_all(keys_needed, acquired_locks)) {
            // Got all locks — execute for real
            Transaction txn(txn_id, db_);
            func(txn);
            txn.flush_writes();
            txn.set_status(TxnStatus::COMMITTED);

            release_all(acquired_locks);
            committed_++;

            auto wall_end = std::chrono::high_resolution_clock::now();
            stats.response_time_ms =
                std::chrono::duration<double, std::milli>(wall_end - wall_start).count();
            return stats;
        }

        // Failed to get all locks — release and wait before retry
        aborted_++;
        stats.retries++;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}
