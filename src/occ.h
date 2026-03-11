#pragma once
#include <mutex>
#include <atomic>
#include "database.h"
#include "transaction.h"

// Stats tracked per transaction run

class OCC {
public:
    explicit OCC(Database& db);

    // Run a transaction function with OCC.
    // The function receives a Transaction& and performs reads/writes.
    // OCC handles validation, abort, and retry automatically.
    // Returns stats about the completed transaction.
    template<typename TxnFunc>
    TxnStats run(int txn_id, TxnFunc func);

    long long get_committed_count() const { return committed_.load(); }
    long long get_aborted_count()   const { return aborted_.load(); }

private:
    Database& db_;
    std::mutex validation_mutex_; // only one txn validates at a time
    std::atomic<long long> committed_{0};
    std::atomic<long long> aborted_{0};

    // Returns true if transaction can commit (no conflicts)
    bool validate(const Transaction& txn);
};

// Template implementation
template<typename TxnFunc>
TxnStats OCC::run(int txn_id, TxnFunc func) {
    TxnStats stats;
    auto wall_start = std::chrono::high_resolution_clock::now();

    while (true) {
        // --- READ PHASE ---
        Transaction txn(txn_id, db_);
        func(txn);  // execute reads and writes (writes go to buffer)

        // --- VALIDATION PHASE (sequential) ---
        {
            std::lock_guard<std::mutex> lock(validation_mutex_);
            if (validate(txn)) {
                // --- WRITE PHASE ---
                txn.flush_writes();
                txn.set_status(TxnStatus::COMMITTED);
                committed_++;

                auto wall_end = std::chrono::high_resolution_clock::now();
                stats.response_time_ms =
                    std::chrono::duration<double, std::milli>(wall_end - wall_start).count();
                return stats;
            }
        }

        // Validation failed — abort and retry
        txn.set_status(TxnStatus::ABORTED);
        aborted_++;
        stats.retries++;

        // Small backoff before retry to reduce contention
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}
