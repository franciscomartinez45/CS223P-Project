#include "twopl.h"

TwoPL::TwoPL(Database& db) : db_(db) {}

bool TwoPL::try_acquire_all(const std::unordered_set<std::string>& keys,
                             std::unordered_set<std::string>& acquired) {
    for (const auto& key : keys) {
        // Get or create the mutex for this key
        std::mutex* mtx = nullptr;
        {
            std::lock_guard<std::mutex> map_lock(map_mutex_);
            mtx = &lock_table_[key];
        }

        // Try to acquire without blocking
        if (mtx->try_lock()) {
            acquired.insert(key);
        } else {
            // Failed — release everything we already acquired
            release_all(acquired);
            return false;
        }
    }
    return true;
}

void TwoPL::release_all(std::unordered_set<std::string>& acquired) {
    std::lock_guard<std::mutex> map_lock(map_mutex_);
    for (const auto& key : acquired) {
        auto it = lock_table_.find(key);
        if (it != lock_table_.end())
            it->second.unlock();
    }
    acquired.clear();
}
