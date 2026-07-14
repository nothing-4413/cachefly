#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "cachefly/resp/resp_value.h"

namespace cachefly::resp {

enum class ParseResult { kComplete, kIncomplete, kError };

class Parser {
public:
    Parser(std::size_t max_bulk_length = 512ULL * 1024ULL * 1024ULL,
           std::size_t max_array_length = 1024ULL * 1024ULL,
           std::size_t max_depth = 128);

    [[nodiscard]] ParseResult Parse(std::string_view input,
                                    std::size_t* consumed,
                                    Value* value,
                                    std::string* error) const;
    [[nodiscard]] ParseResult ParseCommand(std::string_view input,
                                           std::size_t* consumed,
                                           std::vector<std::string>* arguments,
                                           std::string* error) const;

private:
    [[nodiscard]] ParseResult ParseValue(std::string_view input,
                                         std::size_t* cursor,
                                         std::size_t depth,
                                         Value* value,
                                         std::string* error) const;

    std::size_t max_bulk_length_;
    std::size_t max_array_length_;
    std::size_t max_depth_;
};

}  // namespace cachefly::resp
