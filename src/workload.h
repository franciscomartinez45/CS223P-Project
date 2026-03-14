#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include "database.h"
#include "transaction.h"
#include "occ.h"
#include "twopl.h"

struct TransactionType {
    std::string name;
    std::vector<std::string> input_vars;
    TransactionFunc function;
};

struct WorkloadStats {
    long long committed   = 0;
    long long retries     = 0;
    double total_time_ms  = 0.0;
    double elapsed_sec    = 0.0;
    std::vector<double> response_times;
    std::unordered_map<std::string, std::vector<double>> per_type_times;

    double throughput()      const { return elapsed_sec > 0 ? committed / elapsed_sec : 0.0; }
    double avg_response_ms() const { return committed > 0 ? total_time_ms / committed : 0.0; }
    double retry_rate()      const {
        long long total = committed + retries;
        return total > 0 ? 100.0 * retries / total : 0.0;
    }
};

class WorkloadRunner {
public:
    WorkloadRunner(Database& database);

    void add_transaction(const std::string& name,
                         const std::vector<std::string>& input_vars,
                         TransactionFunc function);

    void set_contention(double hot_probability, double hot_fraction);

    void build_key_pools(const std::vector<std::string>& prefixes);

    WorkloadStats run_occ(int num_threads, int transactions_per_thread);

    WorkloadStats run_2pl(int num_threads, int transactions_per_thread);

    void print_stats(const std::string& label, const WorkloadStats& stats);

    void save_csv(const std::string& filepath,
                  const std::string& protocol,
                  const std::string& workload,
                  int threads,
                  double hot_probability,
                  const WorkloadStats& stats);

private:
    Database& db_;
    std::vector<TransactionType> transaction_types_;
    std::unordered_map<std::string, std::vector<std::string>> key_pools_;
    double hot_prob_ = 0.0;
    double hot_frac_ = 0.1;

    std::string prefix_for_var(const std::string& input_var);
    std::string pick_key(const std::string& input_var, std::mt19937& random_engine);

    template<typename Protocol>
    WorkloadStats run_impl(Protocol& protocol, int num_threads, int transactions_per_thread);
};

template<typename Protocol>
WorkloadStats WorkloadRunner::run_impl(Protocol& protocol, int num_threads, int transactions_per_thread) {
    WorkloadStats stats;
    std::mutex stats_mutex;
    std::atomic<int> transaction_id_counter{0};
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> worker_threads;
    for (int i = 0; i < num_threads; i++) {
        worker_threads.emplace_back([&]() {
            std::mt19937 random_engine(std::random_device{}());
            std::uniform_int_distribution<int> type_dist(0, transaction_types_.size() - 1);

            for (int j = 0; j < transactions_per_thread; j++) {
                const TransactionType& transaction_type = transaction_types_[type_dist(random_engine)];

                std::unordered_map<std::string, std::string> key_map;
                for (const auto& input_var : transaction_type.input_vars)
                    key_map[input_var] = pick_key(input_var, random_engine);

                int transaction_id = transaction_id_counter++;
                auto run_stats = protocol.run(transaction_id, [&](Transaction& transaction) {
                    transaction_type.function(transaction, key_map);
                });

                std::lock_guard<std::mutex> lock(stats_mutex);
                stats.committed++;
                stats.retries       += run_stats.retries;
                stats.total_time_ms += run_stats.response_time_ms;
                stats.response_times.push_back(run_stats.response_time_ms);
                stats.per_type_times[transaction_type.name].push_back(run_stats.response_time_ms);
            }
        });
    }
    for (auto& worker_thread : worker_threads) worker_thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();
    return stats;
}
