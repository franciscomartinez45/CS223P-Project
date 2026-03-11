#pragma once
#include <string>
#include "database.h"

class Loader {
public:
    explicit Loader(Database& db);
    int load(const std::string& filepath);
private:
    Database& db_;
};
