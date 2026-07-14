#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cachefly::resp {

enum class Type { kSimpleString, kError, kInteger, kBulkString, kArray, kNull };

struct Value {
    Type type{Type::kNull};
    std::string string;
    std::int64_t integer{0};
    std::vector<Value> array;

    [[nodiscard]] static Value Simple(std::string value);
    [[nodiscard]] static Value Error(std::string value);
    [[nodiscard]] static Value Integer(std::int64_t value);
    [[nodiscard]] static Value Bulk(std::string value);
    [[nodiscard]] static Value Array(std::vector<Value> value);
    [[nodiscard]] static Value Null();
    [[nodiscard]] std::string Encode() const;
};

[[nodiscard]] std::string EncodeCommand(const std::vector<std::string>& arguments);
[[nodiscard]] std::string SanitizeError(std::string_view message);

}  // namespace cachefly::resp
