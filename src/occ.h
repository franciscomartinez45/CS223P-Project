#pragma once
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include "database.h"
#include "transaction.h"

class OCC {
public:
    explicit OCC(Database& database) : db_(database) {}

    template<typename Callable>
    TransactionStats run(int transaction_id, Callable transaction_func);

    long long get_committed_count() const { return committed_.load(); }
    long long get_aborted_count()   const { return aborted_.load(); }

private:
    Database& db_;
    std::mutex validation_mutex_;
    std::atomic<long long> committed_{0};
    std::atomic<long long> aborted_{0};

    bool validate(const Transaction& transaction) {
        for (const auto& entry : transaction.get_read_set()) {
            auto current_value = db_.get(entry.key);
            if (!current_value) return false;
            if (*current_value != entry.value_at_read) return false;
        }
        return true;
    }
};

template<typename Callable>
TransactionStats OCC::run(int transaction_id, Callable transaction_func) {
    TransactionStats stats;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (true) {
        Transaction transaction(transaction_id, db_);
        transaction_func(transaction);

        {
            std::lock_guard<std::mutex> lock(validation_mutex_);
            if (validate(transaction)) {
                transaction.flush_writes();
                transaction.set_status(TransactionStatus::COMMITTED);
                committed_++;
                auto end_time = std::chrono::high_resolution_clock::now();
                stats.response_time_ms =
                    std::chrono::duration<double, std::milli>(end_time - start_time).count();
                return stats;
            }
        }

        transaction.set_status(TransactionStatus::ABORTED);
        aborted_++;
        stats.retries++;

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}
