#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <functional>
#include "database.h"
#include "record.h"

struct ReadEntry {
    std::string key;
    std::string value_at_read;
};

struct TransactionStats {
    double response_time_ms = 0.0;
    int retries = 0;
};

enum class TransactionStatus {
    ACTIVE,
    COMMITTED,
    ABORTED
};

class Transaction {
public:
    explicit Transaction(int transaction_id, Database& database);
    Record read(const std::string& key);
    void write(const std::string& key, const Record& record);
    void flush_writes();
    void set_status(TransactionStatus status) { status_ = status; }
    TransactionStatus get_status() const { return status_; }
    int get_id() const { return id_; } 
    const std::vector<ReadEntry>& get_read_set() const { return read_set_; }
    const std::unordered_map<std::string, std::string>& get_write_buffer() const {
        return write_buffer_;
    }
    std::unordered_set<std::string> get_all_keys() const;
    void start_timer();
    double elapsed_ms() const;

private:

    int id_;
    Database& db_;
    TransactionStatus status_;
    std::vector<ReadEntry> read_set_;
    std::unordered_map<std::string, std::string> write_buffer_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
};

using TransactionFunc = std::function<void(Transaction&, const std::unordered_map<std::string, std::string>&)>;
