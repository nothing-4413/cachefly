#include "cachefly/resp/resp_parser.h"

#include <charconv>
#include <cstdint>
#include <system_error>
#include <utility>

namespace cachefly::resp {
namespace {

ParseResult ReadLine(std::string_view input,
                     std::size_t* cursor,
                     std::string_view* line,
                     std::string* error) {
    const std::size_t end = input.find("\r\n", *cursor);
    if (end == std::string_view::npos) return ParseResult::kIncomplete;
    *line = input.substr(*cursor, end - *cursor);
    *cursor = end + 2;
    if (line->find('\r') != std::string_view::npos ||
        line->find('\n') != std::string_view::npos) {
        *error = "invalid line terminator";
        return ParseResult::kError;
    }
    return ParseResult::kComplete;
}

bool ParseInteger(std::string_view text, std::int64_t* value) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), *value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

}  // namespace

Parser::Parser(std::size_t max_bulk_length,
               std::size_t max_array_length,
               std::size_t max_depth)
    : max_bulk_length_(max_bulk_length),
      max_array_length_(max_array_length),
      max_depth_(max_depth) {}

ParseResult Parser::Parse(std::string_view input,
                          std::size_t* consumed,
                          Value* value,
                          std::string* error) const {
    *consumed = 0;
    error->clear();
    std::size_t cursor = 0;
    const ParseResult result = ParseValue(input, &cursor, 0, value, error);
    if (result == ParseResult::kComplete) *consumed = cursor;
    return result;
}

ParseResult Parser::ParseCommand(std::string_view input,
                                 std::size_t* consumed,
                                 std::vector<std::string>* arguments,
                                 std::string* error) const {
    Value value;
    const ParseResult result = Parse(input, consumed, &value, error);
    if (result != ParseResult::kComplete) return result;
    if (value.type != Type::kArray || value.array.empty()) {
        *error = "command must be a non-empty array";
        *consumed = 0;
        return ParseResult::kError;
    }
    arguments->clear();
    arguments->reserve(value.array.size());
    for (Value& child : value.array) {
        if (child.type != Type::kBulkString && child.type != Type::kSimpleString) {
            arguments->clear();
            *error = "command arguments must be strings";
            *consumed = 0;
            return ParseResult::kError;
        }
        arguments->push_back(std::move(child.string));
    }
    return ParseResult::kComplete;
}

ParseResult Parser::ParseValue(std::string_view input,
                               std::size_t* cursor,
                               std::size_t depth,
                               Value* value,
                               std::string* error) const {
    if (depth > max_depth_) {
        *error = "RESP nesting depth exceeded";
        return ParseResult::kError;
    }
    if (*cursor >= input.size()) return ParseResult::kIncomplete;

    const char prefix = input[(*cursor)++];
    if (prefix == '+' || prefix == '-' || prefix == ':') {
        std::string_view line;
        const ParseResult line_result = ReadLine(input, cursor, &line, error);
        if (line_result != ParseResult::kComplete) return line_result;
        if (prefix == '+') *value = Value::Simple(std::string(line));
        else if (prefix == '-') *value = Value::Error(std::string(line));
        else {
            std::int64_t integer = 0;
            if (!ParseInteger(line, &integer)) {
                *error = "invalid RESP integer";
                return ParseResult::kError;
            }
            *value = Value::Integer(integer);
        }
        return ParseResult::kComplete;
    }

    if (prefix == '$') {
        std::string_view line;
        const ParseResult line_result = ReadLine(input, cursor, &line, error);
        if (line_result != ParseResult::kComplete) return line_result;
        std::int64_t length = 0;
        if (!ParseInteger(line, &length) || length < -1) {
            *error = "invalid bulk string length";
            return ParseResult::kError;
        }
        if (length == -1) {
            *value = Value::Null();
            return ParseResult::kComplete;
        }
        if (static_cast<std::uint64_t>(length) > max_bulk_length_) {
            *error = "bulk string exceeds limit";
            return ParseResult::kError;
        }
        const std::size_t size = static_cast<std::size_t>(length);
        const std::size_t remaining = input.size() - *cursor;
        if (remaining < size || remaining - size < 2) return ParseResult::kIncomplete;
        if (input[*cursor + size] != '\r' || input[*cursor + size + 1] != '\n') {
            *error = "bulk string is missing CRLF";
            return ParseResult::kError;
        }
        *value = Value::Bulk(std::string(input.substr(*cursor, size)));
        *cursor += size + 2;
        return ParseResult::kComplete;
    }

    if (prefix == '*') {
        std::string_view line;
        const ParseResult line_result = ReadLine(input, cursor, &line, error);
        if (line_result != ParseResult::kComplete) return line_result;
        std::int64_t length = 0;
        if (!ParseInteger(line, &length) || length < -1) {
            *error = "invalid array length";
            return ParseResult::kError;
        }
        if (length == -1) {
            *value = Value::Null();
            return ParseResult::kComplete;
        }
        if (static_cast<std::uint64_t>(length) > max_array_length_) {
            *error = "array length exceeds limit";
            return ParseResult::kError;
        }
        std::vector<Value> elements;
        elements.reserve(static_cast<std::size_t>(length));
        for (std::int64_t index = 0; index < length; ++index) {
            Value child;
            const ParseResult child_result = ParseValue(input, cursor, depth + 1, &child, error);
            if (child_result != ParseResult::kComplete) return child_result;
            elements.push_back(std::move(child));
        }
        *value = Value::Array(std::move(elements));
        return ParseResult::kComplete;
    }

    *error = "unknown RESP type byte";
    return ParseResult::kError;
}

}  // namespace cachefly::resp
