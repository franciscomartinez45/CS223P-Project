#include "workload_interp.h"
#include <fstream>
#include <sstream>
#include <algorithm>

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end   = str.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return str.substr(start, end - start + 1);
}

static std::shared_ptr<Expression> parse_simple_expression(const std::string& str) {
    auto expression = std::make_shared<Expression>();
    auto bracket = str.find("[\"");
    if (bracket != std::string::npos) {
        expression->kind  = Expression::FIELD;
        expression->name  = trim(str.substr(0, bracket));
        auto close = str.find("\"]", bracket + 2);
        expression->field = str.substr(bracket + 2, close - bracket - 2);
    } else {
        expression->kind = Expression::SCALAR;
        expression->name = trim(str);
    }
    return expression;
}

static std::shared_ptr<Expression> parse_expression(const std::string& raw) {
    std::string str = trim(raw);

    size_t op_pos = std::string::npos;
    char   op     = '+';
    bool   in_bracket = false;

    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '[') { in_bracket = true;  continue; }
        if (str[i] == ']') { in_bracket = false; continue; }
        if (!in_bracket && i > 0 && str[i - 1] == ' ') {
            if (str[i] == '+' || str[i] == '-') {
                op_pos = i;
                op     = str[i];
            }
        }
    }

    if (op_pos != std::string::npos) {
        auto expression    = std::make_shared<Expression>();
        expression->kind   = Expression::ARITH;
        expression->lhs    = parse_simple_expression(trim(str.substr(0, op_pos - 1)));
        expression->op     = op;
        expression->literal = std::stoi(trim(str.substr(op_pos + 1)));
        return expression;
    }
    return parse_simple_expression(str);
}

static Statement parse_statement_line(const std::string& line) {
    if (line.rfind("WRITE(", 0) == 0) {
        auto paren_end = line.find(')');
        std::string inner = line.substr(6, paren_end - 6);
        auto comma = inner.find(',');
        Statement statement;
        statement.kind      = Statement::WRITE;
        statement.key_param = trim(inner.substr(0, comma));
        statement.variable       = trim(inner.substr(comma + 1));
        return statement;
    }

    auto eq = line.find(" = ");
    std::string lhs = trim(line.substr(0, eq));
    std::string rhs = trim(line.substr(eq + 3));

    if (rhs.rfind("READ(", 0) == 0) {
        auto paren_end = rhs.find(')');
        Statement statement;
        statement.kind      = Statement::READ;
        statement.variable       = lhs;
        statement.key_param = rhs.substr(5, paren_end - 5);
        return statement;
    }

    auto bracket = lhs.find("[\"");
    if (bracket != std::string::npos) {
        auto close = lhs.find("\"]", bracket + 2);
        Statement statement;
        statement.kind  = Statement::SET_FIELD;
        statement.variable   = lhs.substr(0, bracket);
        statement.field = lhs.substr(bracket + 2, close - bracket - 2);
        statement.expression  = parse_expression(rhs);
        return statement;
    }

    Statement statement;
    statement.kind = Statement::ASSIGN_SCALAR;
    statement.variable  = lhs;
    statement.expression = parse_expression(rhs);
    return statement;
}

std::vector<ParsedTransaction> parse_workload_file(const std::string& path) {
    std::ifstream file(path);
    std::vector<ParsedTransaction> result;
    ParsedTransaction* current = nullptr;
    int transaction_count = 0;
    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line == "WORKLOAD") continue;
        if (line == "END") break;
        if (line == "COMMIT") { current = nullptr; continue; }

        if (line.rfind("TRANSACTION", 0) == 0) {
            result.emplace_back();
            current = &result.back();
            transaction_count++;
            current->name = "Txn" + std::to_string(transaction_count);

            auto colon = line.find(':');
            if (colon != std::string::npos) {
                auto paren_end = line.rfind(')');
                std::string inputs_section = line.substr(colon + 1, paren_end - colon - 1);
                std::stringstream stream(inputs_section);
                std::string token;
                while (std::getline(stream, token, ','))
                    current->inputs.push_back(trim(token));
            }
            continue;
        }

        if (!current) continue;

      
        current->statements.push_back(parse_statement_line(line));
        
    }
    return result;
}

static int eval_expression(const Expression& expression,
                            const std::unordered_map<std::string, Record>& records,
                            const std::unordered_map<std::string, int>& scalars) {
    switch (expression.kind) {
    case Expression::FIELD:
        return records.at(expression.name).get_int(expression.field);
    case Expression::SCALAR:
        return scalars.at(expression.name);
    case Expression::ARITH: {
        int value = eval_expression(*expression.lhs, records, scalars);
        return expression.op == '+' ? value + expression.literal : value - expression.literal;
    }
    default:
        return 0;
    }
}

struct TransactionExecutor {
    std::vector<Statement> statements;

    void operator()(Transaction& transaction,
                    const std::unordered_map<std::string,std::string>& keys) const {
        std::unordered_map<std::string, Record> records;
        std::unordered_map<std::string, int>    scalars;

        for (const auto& statement : statements) {
            switch (statement.kind) {
            case Statement::READ:
                records[statement.variable] = transaction.read(keys.at(statement.key_param));
                break;
            case Statement::WRITE:
                transaction.write(keys.at(statement.key_param), records.at(statement.variable));
                break;
            case Statement::SET_FIELD:
                records.at(statement.variable).set(statement.field,
                    eval_expression(*statement.expression, records, scalars));
                break;
            case Statement::ASSIGN_SCALAR:
                scalars[statement.variable] = eval_expression(*statement.expression, records, scalars);
                break;
            }
        }
    }
};

TransactionFunc build_transaction_function(const ParsedTransaction& parsed_transaction) {
    TransactionExecutor executor;
    executor.statements = parsed_transaction.statements;
    return executor;
}
