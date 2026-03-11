#include "transaction.h"
#include <stdexcept>

Transaction::Transaction(int id, Database& db)
    : id_(id), db_(db), status_(TxnStatus::ACTIVE) {
    start_timer();
}

Record Transaction::read(const std::string& key) {
    auto it = write_buffer_.find(key);
    if (it != write_buffer_.end())
        return Record::deserialize(it->second);

    auto raw = db_.get(key);
    if (!raw)
        throw std::runtime_error("Transaction " + std::to_string(id_) +
                                 ": key not found: " + key);
    read_set_.push_back({key, *raw});
    return Record::deserialize(*raw);
}

void Transaction::write(const std::string& key, const Record& value) {
    write_buffer_[key] = value.serialize();
}

void Transaction::flush_writes() {
    for (const auto& [key, value] : write_buffer_)
        db_.put(key, value);
}

std::unordered_set<std::string> Transaction::get_all_keys() const {
    std::unordered_set<std::string> keys;
    for (const auto& entry : read_set_)
        keys.insert(entry.key);
    for (const auto& [key, _] : write_buffer_)
        keys.insert(key);
    return keys;
}

void Transaction::start_timer() {
    start_time_ = std::chrono::high_resolution_clock::now();
}

double Transaction::elapsed_ms() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(now - start_time_).count();
}
