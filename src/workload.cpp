#include "workload.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <numeric>

WorkloadRunner::WorkloadRunner(Database& database) : db_(database) {}

void WorkloadRunner::add_transaction(const std::string& name,
                                     const std::vector<std::string>& input_vars,
                                     TransactionFunc function) {
    transaction_types_.push_back({name, input_vars, function});
}

void WorkloadRunner::set_contention(double hot_probability, double hot_fraction) {
    hot_prob_ = hot_probability;
    hot_frac_ = hot_fraction;
}

void WorkloadRunner::build_key_pools(const std::vector<std::string>& prefixes) {
    for (const auto& prefix : prefixes)
        key_pools_[prefix] = {};
    db_.for_each_key([&](const std::string& key) {
        auto underscore = key.find('_');
        if (underscore == std::string::npos) return;
        std::string prefix = key.substr(0, underscore);
        auto it = key_pools_.find(prefix);
        if (it != key_pools_.end())
            it->second.push_back(key);
    });
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

std::string WorkloadRunner::prefix_for_var(const std::string& input_var) {
    if (input_var[0] == 'W') return "W";
    if (input_var[0] == 'D') return "D";
    if (input_var[0] == 'C') return "C";
    if (input_var[0] == 'S') return "S";
    return "A";
}

std::string WorkloadRunner::pick_key(const std::string& input_var, std::mt19937& random_engine) {
    std::string prefix   = prefix_for_var(input_var);
    const auto& pool     = key_pools_.at(prefix);
    int pool_size        = pool.size();
    int hot_size         = std::max(1, (int)(pool_size * hot_frac_));

    std::uniform_real_distribution<double> coin(0.0, 1.0);
    if (coin(random_engine) < hot_prob_) {
        std::uniform_int_distribution<int> hot_dist(0, hot_size - 1);
        return pool[hot_dist(random_engine)];
    } else {
        std::uniform_int_distribution<int> full_dist(0, pool_size - 1);
        return pool[full_dist(random_engine)];
    }
}

WorkloadStats WorkloadRunner::run_occ(int num_threads, int transactions_per_thread) {
    OCC protocol(db_);
    return run_impl(protocol, num_threads, transactions_per_thread);
}

WorkloadStats WorkloadRunner::run_2pl(int num_threads, int transactions_per_thread) {
    TwoPL protocol(db_);
    return run_impl(protocol, num_threads, transactions_per_thread);
}

void WorkloadRunner::save_csv(const std::string& filepath,
                              const std::string& protocol,
                              const std::string& workload,
                              int threads,
                              double hot_probability,
                              const WorkloadStats& stats) {
    std::ifstream check(filepath);
    bool write_header = !check.good() || check.peek() == std::ifstream::traits_type::eof();
    check.close();

    std::ofstream output_file(filepath, std::ios::app);
    if (write_header)
        output_file << "workload,protocol,threads,hot_prob,committed,retries,retry_rate_pct,"
                       "throughput_tps,avg_resp_ms,p50_resp_ms,p95_resp_ms,max_resp_ms\n";

    double p50_ms = 0, p95_ms = 0, max_ms = 0;
    if (!stats.response_times.empty()) {
        auto sorted_times = stats.response_times;
        std::sort(sorted_times.begin(), sorted_times.end());
        p50_ms = sorted_times[sorted_times.size() * 50 / 100];
        p95_ms = sorted_times[sorted_times.size() * 95 / 100];
        max_ms = sorted_times.back();
    }

    output_file << workload << ","
                << protocol << ","
                << threads << ","
                << hot_probability << ","
                << stats.committed << ","
                << stats.retries << ","
                << stats.retry_rate() << ","
                << stats.throughput() << ","
                << stats.avg_response_ms() << ","
                << p50_ms << ","
                << p95_ms << ","
                << max_ms << "\n";
}

void WorkloadRunner::print_stats(const std::string& label, const WorkloadStats& stats) {
    std::cout << "  [" << label << "]\n";
    std::cout << "    Committed  : " << stats.committed << "\n";
    std::cout << "    Retries    : " << stats.retries
              << " (" << stats.retry_rate() << "%)\n";
    std::cout << "    Throughput : " << stats.throughput() << " transactions/sec\n";
    std::cout << "    Avg Resp   : " << stats.avg_response_ms() << " ms\n";
    if (!stats.response_times.empty()) {
        auto sorted_times = stats.response_times;
        std::sort(sorted_times.begin(), sorted_times.end());
        std::cout << "    Max Resp   : " << sorted_times.back() << " ms\n";
        std::cout << "    P50 Resp   : " << sorted_times[sorted_times.size() * 50 / 100] << " ms\n";
        std::cout << "    P95 Resp   : " << sorted_times[sorted_times.size() * 95 / 100] << " ms\n";
    }
    if (stats.per_type_times.size() > 1) {
        for (const auto& [name, times] : stats.per_type_times) {
            if (times.empty()) continue;
            double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
            std::cout << "    [" << name << "] count=" << times.size()
                      << " avg=" << avg << " ms\n";
        }
    }
}
