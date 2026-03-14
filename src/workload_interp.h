#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include "transaction.h"


struct Expression {
    enum Kind { FIELD, SCALAR, ARITH } kind;
    std::string name;
    std::string field;
    std::shared_ptr<Expression> lhs;
    char op = '+';
    int  literal = 0;
};

struct Statement {
    enum Kind { READ, WRITE, SET_FIELD, ASSIGN_SCALAR } kind;
    std::string variable;
    std::string key_param;
    std::string field;
    std::shared_ptr<Expression> expression;
};

struct ParsedTransaction {
    std::string name;
    std::vector<std::string> inputs;
    std::vector<Statement> statements;
};

std::vector<ParsedTransaction> parse_workload_file(const std::string& path);

TransactionFunc build_transaction_function(const ParsedTransaction& parsed_transaction);

inline std::string prefix_for_input(const std::string& input_name) {
    if (!input_name.empty()) {
        switch (input_name[0]) {
            case 'W': return "W";
            case 'D': return "D";
            case 'C': return "C";
            case 'S': return "S";
        }
    }
    return "A";
}
