#include "database.h"
#include <stdexcept>
#include <rocksdb/iterator.h>

Database::Database(const std::string& db_path) {
    rocksdb::Options options;
    options.create_if_missing = true;
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    auto status = rocksdb::DB::Open(options, db_path, &db_);
    check(status, "opening database at " + db_path);
}

void Database::put(const std::string& key, const std::string& value) {
    check(db_->Put(rocksdb::WriteOptions(), key, value), "put(" + key + ")");
}

std::optional<std::string> Database::get(const std::string& key) {
    std::string value;
    auto status = db_->Get(rocksdb::ReadOptions(), key, &value);
    if (status.IsNotFound()) return std::nullopt;
    check(status, "get(" + key + ")");
    return value;
}

void Database::remove(const std::string& key) {
    check(db_->Delete(rocksdb::WriteOptions(), key), "remove(" + key + ")");
}

bool Database::exists(const std::string& key) {
    return get(key).has_value();
}

void Database::for_each_key(const std::function<void(const std::string&)>& callback) {
    rocksdb::Iterator* it = db_->NewIterator(rocksdb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next())
        callback(it->key().ToString());
    delete it;
}

void Database::check(const rocksdb::Status& status, const std::string& context) {
    if (!status.ok())
        throw std::runtime_error("RocksDB error during " + context +
                                 ": " + status.ToString());
}
