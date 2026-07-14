#include "cachefly/resp/resp_value.h"

#include <utility>

namespace cachefly::resp {

Value Value::Simple(std::string value) {
    Value result;
    result.type = Type::kSimpleString;
    result.string = std::move(value);
    return result;
}

Value Value::Error(std::string value) {
    Value result;
    result.type = Type::kError;
    result.string = std::move(value);
    return result;
}

Value Value::Integer(std::int64_t value) {
    Value result;
    result.type = Type::kInteger;
    result.integer = value;
    return result;
}

Value Value::Bulk(std::string value) {
    Value result;
    result.type = Type::kBulkString;
    result.string = std::move(value);
    return result;
}

Value Value::Array(std::vector<Value> value) {
    Value result;
    result.type = Type::kArray;
    result.array = std::move(value);
    return result;
}

Value Value::Null() { return {}; }

std::string Value::Encode() const {
    switch (type) {
        case Type::kSimpleString:
            return "+" + string + "\r\n";
        case Type::kError:
            return "-" + SanitizeError(string) + "\r\n";
        case Type::kInteger:
            return ":" + std::to_string(integer) + "\r\n";
        case Type::kBulkString:
            return "$" + std::to_string(string.size()) + "\r\n" + string + "\r\n";
        case Type::kArray: {
            std::string encoded = "*" + std::to_string(array.size()) + "\r\n";
            for (const Value& child : array) encoded += child.Encode();
            return encoded;
        }
        case Type::kNull:
            return "$-1\r\n";
    }
    return "-ERR unsupported RESP value\r\n";
}

std::string EncodeCommand(const std::vector<std::string>& arguments) {
    std::vector<Value> values;
    values.reserve(arguments.size());
    for (const std::string& argument : arguments) values.push_back(Value::Bulk(argument));
    return Value::Array(std::move(values)).Encode();
}

std::string SanitizeError(std::string_view message) {
    std::string result(message);
    for (char& character : result) {
        if (character == '\r' || character == '\n') character = ' ';
    }
    return result;
}

}  // namespace cachefly::resp
