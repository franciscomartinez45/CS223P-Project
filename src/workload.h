#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <random>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include "database.h"
#include "transaction.h"
#include "occ.h"
#include "twopl.h"

// A transaction function takes a Transaction& and a map of input keys
using TxnFunc = std::function<void(Transaction&, const std::unordered_map<std::string,std::string>&)>;

// One registered transaction type
struct TxnType {
    std::string name;
    std::vector<std::string> input_vars; // e.g. ["FROM_KEY", "TO_KEY"]
    TxnFunc func;
};

// Statistics collected during a workload run
struct WorkloadStats {
    long long committed   = 0;
    long long retries     = 0;
    double total_time_ms  = 0.0;
    double elapsed_sec    = 0.0;
    std::vector<double> response_times;
    std::unordered_map<std::string, std::vector<double>> per_type_times;
    std::shared_ptr<std::mutex> mtx = std::make_shared<std::mutex>();

    double throughput()       const { return elapsed_sec > 0 ? committed / elapsed_sec : 0.0; }
    double avg_response_ms()  const { return committed > 0 ? total_time_ms / committed : 0.0; }
    double retry_rate()       const {
        long long total = committed + retries;
        return total > 0 ? 100.0 * retries / total : 0.0;
    }
};

class WorkloadRunner {
public:
    WorkloadRunner(Database& db);

    // Register a transaction type
    void add_transaction(const std::string& name,
                         const std::vector<std::string>& input_vars,
                         TxnFunc func);

    // Set contention: hot_prob = chance of picking hot key,
    //                 hot_frac = fraction of keyspace that is hot
    void set_contention(double hot_prob, double hot_frac);

    // Scan DB keys and group by prefix automatically
    void build_key_pools(const std::vector<std::string>& prefixes);

    // Run with OCC
    WorkloadStats run_occ(int num_threads, int txns_per_thread);

    // Run with 2PL
    WorkloadStats run_2pl(int num_threads, int txns_per_thread);

    void print_stats(const std::string& label, const WorkloadStats& s);

    // Append one row of results to a CSV file (creates header on first write)
    void save_csv(const std::string& filepath,
                  const std::string& protocol,
                  const std::string& workload,
                  int threads,
                  double hot_prob,
                  const WorkloadStats& s);

private:
    Database& db_;
    std::vector<TxnType> txn_types_;
    std::unordered_map<std::string, std::vector<std::string>> key_pools_;
    double hot_prob_ = 0.0;
    double hot_frac_ = 0.1;

    std::string prefix_for_var(const std::string& var);
    std::string pick_key(const std::string& input_var, std::mt19937& rng);

    template<typename Protocol>
    WorkloadStats run_impl(Protocol& proto, int num_threads, int txns_per_thread);
};

template<typename Protocol>
WorkloadStats WorkloadRunner::run_impl(Protocol& proto, int num_threads, int txns_per_thread) {
    WorkloadStats stats;
    std::atomic<int> txn_id_counter{0};
    auto wall_start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&]() {
            std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<int> type_dist(0, txn_types_.size() - 1);

            for (int i = 0; i < txns_per_thread; i++) {
                // Pick random transaction type
                const TxnType& txn_type = txn_types_[type_dist(rng)];

                // Assign random keys to each input variable
                std::unordered_map<std::string, std::string> key_map;
                for (const auto& var : txn_type.input_vars)
                    key_map[var] = pick_key(var, rng);

                int tid = txn_id_counter++;
                auto s = proto.run(tid, [&](Transaction& txn) {
                    txn_type.func(txn, key_map);
                });

                std::lock_guard<std::mutex> lock(*stats.mtx);
                stats.committed++;
                stats.retries       += s.retries;
                stats.total_time_ms += s.response_time_ms;
                stats.response_times.push_back(s.response_time_ms);
                stats.per_type_times[txn_type.name].push_back(s.response_time_ms);
            }
        });
    }
    for (auto& th : threads) th.join();

    auto wall_end = std::chrono::high_resolution_clock::now();
    stats.elapsed_sec = std::chrono::duration<double>(wall_end - wall_start).count();
    return stats;
}
