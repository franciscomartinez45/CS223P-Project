#include "workload.h"
#include <iostream>
#include <algorithm>

WorkloadRunner::WorkloadRunner(Database& db) : db_(db) {}

void WorkloadRunner::add_transaction(const std::string& name,
                                     const std::vector<std::string>& input_vars,
                                     TxnFunc func) {
    txn_types_.push_back({name, input_vars, func});
}

void WorkloadRunner::set_contention(double hot_prob, double hot_frac) {
    hot_prob_ = hot_prob;
    hot_frac_ = hot_frac;
}

void WorkloadRunner::build_key_pools(const std::vector<std::string>& prefixes) {
    // Initialize empty pools for requested prefixes
    for (const auto& p : prefixes)
        key_pools_[p] = {};

    // Scan all keys in DB and bucket by prefix
    db_.for_each_key([&](const std::string& key) {
        // Prefix = everything before the first '_'
        auto underscore = key.find('_');
        if (underscore == std::string::npos) return;
        std::string prefix = key.substr(0, underscore);
        auto it = key_pools_.find(prefix);
        if (it != key_pools_.end())
            it->second.push_back(key);
    });

    // Print results and remove empty pools
    for (auto it = key_pools_.begin(); it != key_pools_.end(); ) {
        if (it->second.empty()) {
            it = key_pools_.erase(it);
        } else {
            std::cout << "  " << it->first << "_* : "
                      << it->second.size() << " keys\n";
            ++it;
        }
    }
}

std::string WorkloadRunner::prefix_for_var(const std::string& var) {
    if (var[0] == 'W') return "W";
    if (var[0] == 'D') return "D";
    if (var[0] == 'C') return "C";
    if (var[0] == 'S') return "S";
    return "A";
}

std::string WorkloadRunner::pick_key(const std::string& input_var, std::mt19937& rng) {
    std::string prefix = prefix_for_var(input_var);
    const auto& pool   = key_pools_.at(prefix);
    int pool_size      = pool.size();
    int hot_size       = std::max(1, (int)(pool_size * hot_frac_));

    std::uniform_real_distribution<double> coin(0.0, 1.0);
    if (coin(rng) < hot_prob_) {
        std::uniform_int_distribution<int> d(0, hot_size - 1);
        return pool[d(rng)];
    } else {
        std::uniform_int_distribution<int> d(0, pool_size - 1);
        return pool[d(rng)];
    }
}

WorkloadStats WorkloadRunner::run_occ(int num_threads, int txns_per_thread) {
    OCC proto(db_);
    return run_impl(proto, num_threads, txns_per_thread);
}

WorkloadStats WorkloadRunner::run_2pl(int num_threads, int txns_per_thread) {
    TwoPL proto(db_);
    return run_impl(proto, num_threads, txns_per_thread);
}

void WorkloadRunner::print_stats(const std::string& label, const WorkloadStats& s) {
    std::cout << "  [" << label << "]\n";
    std::cout << "    Committed  : " << s.committed << "\n";
    std::cout << "    Retries    : " << s.retries
              << " (" << s.retry_rate() << "%)\n";
    std::cout << "    Throughput : " << s.throughput() << " txns/sec\n";
    std::cout << "    Avg Resp   : " << s.avg_response_ms() << " ms\n";
}
