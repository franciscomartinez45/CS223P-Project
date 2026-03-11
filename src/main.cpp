#include <iostream>
#include "database.h"
#include "loader.h"
#include "record.h"
#include "transaction.h"
#include "workload.h"

// ── Workload 1 transactions ───────────────────────────────────────────────
void txn_transfer(Transaction& txn,
                  const std::unordered_map<std::string,std::string>& keys) {
    Record from = txn.read(keys.at("FROM_KEY"));
    Record to   = txn.read(keys.at("TO_KEY"));
    from.set("balance", from.get_int("balance") - 1);
    to.set("balance",   to.get_int("balance")   + 1);
    txn.write(keys.at("FROM_KEY"), from);
    txn.write(keys.at("TO_KEY"),   to);
}

// ── Workload 2 transactions ───────────────────────────────────────────────
void txn_new_order(Transaction& txn,
                   const std::unordered_map<std::string,std::string>& keys) {
    Record d = txn.read(keys.at("D_KEY"));
    d.set("next_o_id", d.get_int("next_o_id") + 1);
    txn.write(keys.at("D_KEY"), d);

    for (const auto& sk : {"S_KEY_1", "S_KEY_2", "S_KEY_3"}) {
        Record s = txn.read(keys.at(sk));
        s.set("qty",       s.get_int("qty")       - 1);
        s.set("ytd",       s.get_int("ytd")       + 1);
        s.set("order_cnt", s.get_int("order_cnt") + 1);
        txn.write(keys.at(sk), s);
    }
}

void txn_payment(Transaction& txn,
                 const std::unordered_map<std::string,std::string>& keys) {
    Record w = txn.read(keys.at("W_KEY"));
    w.set("ytd", w.get_int("ytd") + 5);
    txn.write(keys.at("W_KEY"), w);

    Record d = txn.read(keys.at("D_KEY"));
    d.set("ytd", d.get_int("ytd") + 5);
    txn.write(keys.at("D_KEY"), d);

    Record c = txn.read(keys.at("C_KEY"));
    c.set("balance",     c.get_int("balance")     - 5);
    c.set("ytd_payment", c.get_int("ytd_payment") + 5);
    c.set("payment_cnt", c.get_int("payment_cnt") + 1);
    txn.write(keys.at("C_KEY"), c);
}

// ── Helper: run one experiment ────────────────────────────────────────────
void run_experiment(const std::string& wl_name,
                    WorkloadRunner& runner,
                    int threads, int txns,
                    double hot_prob, double hot_frac) {
    runner.set_contention(hot_prob, hot_frac);
    std::cout << "\n=== " << wl_name
              << " | threads=" << threads
              << " hot_prob=" << hot_prob << " ===\n";

    auto occ_stats = runner.run_occ(threads, txns);
    runner.print_stats("OCC", occ_stats);

    auto pl_stats = runner.run_2pl(threads, txns);
    runner.print_stats("2PL", pl_stats);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <1|2>\n";
        return 1;
    }

    int workload_num = std::stoi(argv[2]);

    // ── Workload 1 ────────────────────────────────────────────────────────
    if (workload_num == 1) {
        Database db("/tmp/cs223p_wl1");
        Loader(db).load(argv[1]);

        WorkloadRunner runner(db);
        std::cout << "Building key pools:\n";
        runner.build_key_pools({"A"});

        runner.add_transaction("Transfer",
            {"FROM_KEY", "TO_KEY"}, txn_transfer);

        // Vary threads at fixed contention
        run_experiment("WL1", runner, 1, 200, 0.5, 0.1);
        run_experiment("WL1", runner, 2, 200, 0.5, 0.1);
        run_experiment("WL1", runner, 4, 200, 0.5, 0.1);
        run_experiment("WL1", runner, 8, 200, 0.5, 0.1);

        // Vary contention at fixed threads
        run_experiment("WL1", runner, 4, 200, 0.0, 0.1);
        run_experiment("WL1", runner, 4, 200, 0.3, 0.1);
        run_experiment("WL1", runner, 4, 200, 0.7, 0.1);
        run_experiment("WL1", runner, 4, 200, 1.0, 0.1);
    }

    // ── Workload 2 ────────────────────────────────────────────────────────
    else if (workload_num == 2) {
        Database db("/tmp/cs223p_wl2");
        Loader(db).load(argv[1]);

        WorkloadRunner runner(db);
        std::cout << "Building key pools:\n";
        runner.build_key_pools({"W", "D", "C", "S"});

        runner.add_transaction("NewOrder",
            {"D_KEY", "S_KEY_1", "S_KEY_2", "S_KEY_3"}, txn_new_order);
        runner.add_transaction("Payment",
            {"W_KEY", "D_KEY", "C_KEY"}, txn_payment);

        // Vary threads
        run_experiment("WL2", runner, 1, 200, 0.5, 0.1);
        run_experiment("WL2", runner, 2, 200, 0.5, 0.1);
        run_experiment("WL2", runner, 4, 200, 0.5, 0.1);
        run_experiment("WL2", runner, 8, 200, 0.5, 0.1);

        // Vary contention
        run_experiment("WL2", runner, 4, 200, 0.0, 0.1);
        run_experiment("WL2", runner, 4, 200, 0.3, 0.1);
        run_experiment("WL2", runner, 4, 200, 0.7, 0.1);
        run_experiment("WL2", runner, 4, 200, 1.0, 0.1);
    }

    return 0;
}
