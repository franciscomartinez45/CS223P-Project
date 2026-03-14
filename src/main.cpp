#include <iostream>
#include <fstream>
#include <ctime>
#include <set>
#include <filesystem>
#include "database.h"
#include "record.h"
#include "transaction.h"
#include "workload.h"
#include "workload_interp.h"

static std::string make_timestamp() {
    std::time_t current_time = std::time(nullptr);
    char timestamp_buffer[32];
    std::strftime(timestamp_buffer, sizeof(timestamp_buffer), "%Y-%m-%d_%H-%M-%S", std::localtime(&current_time));
    return timestamp_buffer;
}

void run_benchmark(const std::string& workload_name,
                   WorkloadRunner& runner,
                   int threads, int transactions,
                   double hot_probability, double hot_fraction,
                   const std::string& csv_filepath) {
    runner.set_contention(hot_probability, hot_fraction);
    std::cout << "\n=== " << workload_name
              << " | threads=" << threads
              << " hot_prob=" << hot_probability << " ===\n";

    auto occ_results = runner.run_occ(threads, transactions);
    runner.print_stats("OCC", occ_results);
    runner.save_csv(csv_filepath, "OCC", workload_name, threads, hot_probability, occ_results);

    auto twopl_results = runner.run_2pl(threads, transactions);
    runner.print_stats("2PL", twopl_results);
    runner.save_csv(csv_filepath, "2PL", workload_name, threads, hot_probability, twopl_results);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <data_file> <workload_file>\n";
        return 1;
    }

    std::string data_file     = argv[1];
    std::string workload_file = argv[2];

    std::filesystem::path workload_path(workload_file);
    std::string workload_label = workload_path.stem().string();

    Database db("/tmp/cs223p_" + workload_label);
    {
        std::ifstream file(data_file);
        std::string line;
        std::getline(file, line);
        while (std::getline(file, line)) {
            if (line == "END" || line.empty()) continue;
            auto val_pos = line.find(", VALUE: ");
            db.put(line.substr(5, val_pos - 5), line.substr(val_pos + 9));
        }
    }

    std::cout << "Parsing workload file: " << workload_file << "\n";
    auto parsed_transactions = parse_workload_file(workload_file);
    std::cout << "Found " << parsed_transactions.size() << " transaction type(s).\n";

    std::set<std::string> key_prefixes;
    for (const auto& parsed_transaction : parsed_transactions)
        for (const auto& input : parsed_transaction.inputs)
            key_prefixes.insert(prefix_for_input(input));

    WorkloadRunner runner(db);
    std::cout << "Building key pools:\n";
    runner.build_key_pools(
        std::vector<std::string>(key_prefixes.begin(), key_prefixes.end()));

    for (const auto& parsed_transaction : parsed_transactions)
        runner.add_transaction(parsed_transaction.name, parsed_transaction.inputs, build_transaction_function(parsed_transaction));

    std::string results_directory = "Results/" + workload_label;
    std::filesystem::create_directories(results_directory);
    const std::string csv_filepath = results_directory + "/" + workload_label + "-"
                                   + make_timestamp() + ".csv";

    run_benchmark(workload_label, runner, 1, 200, 0.5, 0.1, csv_filepath);
    run_benchmark(workload_label, runner, 2, 200, 0.5, 0.1, csv_filepath);
    run_benchmark(workload_label, runner, 4, 200, 0.5, 0.1, csv_filepath);
    run_benchmark(workload_label, runner, 8, 200, 0.5, 0.1, csv_filepath);

    run_benchmark(workload_label, runner, 4, 200, 0.0,  0.1, csv_filepath);
    run_benchmark(workload_label, runner, 4, 200, 0.25, 0.1, csv_filepath);
    run_benchmark(workload_label, runner, 4, 200, 0.5,  0.1, csv_filepath);
    run_benchmark(workload_label, runner, 4, 200, 0.75, 0.1, csv_filepath);
    run_benchmark(workload_label, runner, 4, 200, 1.0,  0.1, csv_filepath);

    return 0;
}
