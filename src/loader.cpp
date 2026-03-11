#include "loader.h"
#include <fstream>
#include <stdexcept>

Loader::Loader(Database& db) : db_(db) {}

int Loader::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + filepath);
    std::string line;
    std::getline(file, line);
    if (line != "INSERT")
        throw std::runtime_error("Expected INSERT, got: " + line);
    int count = 0;
    while (std::getline(file, line)) {
        if (line == "END") break;
        if (line.empty()) continue;
        auto key_pos = line.find("KEY: ");
        auto val_pos = line.find(", VALUE: ");
        if (key_pos == std::string::npos || val_pos == std::string::npos)
            throw std::runtime_error("Malformed line: " + line);
        std::string key   = line.substr(5, val_pos - 5);
        std::string value = line.substr(val_pos + 9);
        db_.put(key, value);
        count++;
    }
    return count;
}
