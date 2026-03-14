#pragma once
#include <string>
#include <map>
#include <stdexcept>
#include <sstream>

struct Record {
    std::map<std::string, std::string> fields;

    std::string get_str(const std::string& field_name) const {
        auto it = fields.find(field_name);
        if (it == fields.end())
            throw std::runtime_error("Field not found: " + field_name);
        return it->second;
    }

    int get_int(const std::string& field_name) const {
        return std::stoi(get_str(field_name));
    }

    void set(const std::string& field_name, const std::string& value) {
        fields[field_name] = value;
    }

    void set(const std::string& field_name, int value) {
        fields[field_name] = std::to_string(value);
    }

    std::string serialize() const {
        std::ostringstream output;
        output << "{";
        bool first = true;
        for (const auto& [field_name, value] : fields) {
            if (!first) output << ", ";
            first = false;
            output << field_name << ": " << value;
        }
        output << "}";
        return output.str();
    }

    static Record deserialize(const std::string& raw) {
        Record record;

        auto start = raw.find('{');
        auto end   = raw.rfind('}');
        if (start == std::string::npos || end == std::string::npos)
            throw std::runtime_error("Invalid record format: " + raw);
        std::string body = raw.substr(start + 1, end - start - 1);

        std::vector<std::string> tokens;
        std::string token;
        bool in_quotes = false;
        for (char character : body) {
            if (character == '"') in_quotes = !in_quotes;
            if (character == ',' && !in_quotes) {
                tokens.push_back(token);
                token.clear();
            } else {
                token += character;
            }
        }
        if (!token.empty()) tokens.push_back(token);

        for (const auto& field_token : tokens) {
            auto colon = field_token.find(':');
            if (colon == std::string::npos) continue;
            std::string field_name = trim(field_token.substr(0, colon));
            std::string value      = trim(field_token.substr(colon + 1));
            record.fields[field_name] = value;
        }
        return record;
    }

private:
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        size_t end   = str.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return str.substr(start, end - start + 1);
    }
};
