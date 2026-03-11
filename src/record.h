#pragma once
#include <string>
#include <map>
#include <stdexcept>
#include <sstream>

// Generic record: stores any set of fields as strings.
// Integer fields are stored as strings and converted on access.
// Handles the format: {name: "Account-1", balance: 153, w_id: 1}
struct Record {
    std::map<std::string, std::string> fields;

    // Get a string field
    std::string get_str(const std::string& key) const {
        auto it = fields.find(key);
        if (it == fields.end())
            throw std::runtime_error("Field not found: " + key);
        return it->second;
    }

    // Get an integer field
    int get_int(const std::string& key) const {
        return std::stoi(get_str(key));
    }

    // Set a string field
    void set(const std::string& key, const std::string& val) {
        fields[key] = val;
    }

    // Set an integer field
    void set(const std::string& key, int val) {
        fields[key] = std::to_string(val);
    }

    // Serialize back to {field: value, ...} format
    std::string serialize() const {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (const auto& [k, v] : fields) {
            if (!first) oss << ", ";
            first = false;
            // If value looks like a quoted string, keep quotes; else write raw
            if (!v.empty() && v[0] == '"')
                oss << k << ": " << v;
            else
                oss << k << ": " << v;
        }
        oss << "}";
        return oss.str();
    }

    // Parse from stored string: {name: "Account-1", balance: 153}
    static Record deserialize(const std::string& s) {
        Record r;
        // Strip outer braces
        auto start = s.find('{');
        auto end   = s.rfind('}');
        if (start == std::string::npos || end == std::string::npos)
            throw std::runtime_error("Invalid record format: " + s);
        std::string body = s.substr(start + 1, end - start - 1);

        // Split on commas, but NOT commas inside quotes
        std::vector<std::string> tokens;
        std::string token;
        bool in_quotes = false;
        for (char c : body) {
            if (c == '"') in_quotes = !in_quotes;
            if (c == ',' && !in_quotes) {
                tokens.push_back(token);
                token.clear();
            } else {
                token += c;
            }
        }
        if (!token.empty()) tokens.push_back(token);

        // Parse each "key: value" token
        for (const auto& t : tokens) {
            auto colon = t.find(':');
            if (colon == std::string::npos) continue;
            std::string key = trim(t.substr(0, colon));
            std::string val = trim(t.substr(colon + 1));
            r.fields[key] = val;
        }
        return r;
    }

private:
    static std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        return s.substr(a, b - a + 1);
    }
};
