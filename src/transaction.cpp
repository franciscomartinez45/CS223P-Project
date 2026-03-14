#include "transaction.h"

Transaction::Transaction(int transaction_id, Database& database)
    : id_(transaction_id), db_(database), status_(TransactionStatus::ACTIVE) {
    start_timer();
}

Record Transaction::read(const std::string& key) {
    auto it = write_buffer_.find(key);
    if (it != write_buffer_.end())
        return Record::deserialize(it->second);

    auto raw_value = db_.get(key);
    read_set_.push_back({key, *raw_value});
    return Record::deserialize(*raw_value);
}

void Transaction::write(const std::string& key, const Record& record) {
    write_buffer_[key] = record.serialize();
}

void Transaction::flush_writes() {
    for (const auto& [key, value] : write_buffer_)
        db_.put(key, value);
}

std::unordered_set<std::string> Transaction::get_all_keys() const {
    std::unordered_set<std::string> all_keys;
    for (const auto& entry : read_set_)
        all_keys.insert(entry.key);
    for (const auto& [key, value] : write_buffer_)
        all_keys.insert(key);
    return all_keys;
}

void Transaction::start_timer() {
    start_time_ = std::chrono::high_resolution_clock::now();
}

double Transaction::elapsed_ms() const {
    auto current_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(current_time - start_time_).count();
}
