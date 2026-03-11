#include "occ.h"

OCC::OCC(Database& db) : db_(db) {}

bool OCC::validate(const Transaction& txn) {
    // For each key in the read set, check the current DB value
    // matches what was read. If anything changed, conflict detected.
    for (const auto& entry : txn.get_read_set()) {
        auto current = db_.get(entry.key);

        // Key was deleted since we read it
        if (!current) return false;

        // Value was modified since we read it
        if (*current != entry.value_at_read) return false;
    }
    return true;
}
