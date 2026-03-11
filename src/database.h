#pragma once
#include <string>
#include <memory>
#include <optional>
#include <functional>
#include <rocksdb/db.h>
#include <rocksdb/options.h>

class Database {
public:
    explicit Database(const std::string& db_path);
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    void remove(const std::string& key);
    bool exists(const std::string& key);

    // Iterate all keys in the database
    void for_each_key(const std::function<void(const std::string&)>& callback);

private:
    std::unique_ptr<rocksdb::DB> db_;
    void check(const rocksdb::Status& status, const std::string& context);
};
