#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cachefly/resp/resp_value.h"

namespace cachefly::command {

struct CommandSpec {
    std::string name;
    std::size_t minimum_arguments{0};
    std::optional<std::size_t> maximum_arguments;
    bool mutating{false};
    std::function<resp::Value(const std::vector<std::string>&)> handler;
};

class CommandRegistry {
public:
    void Register(CommandSpec command);
    [[nodiscard]] const CommandSpec* Find(const std::string& uppercase_name) const;
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    std::unordered_map<std::string, CommandSpec> commands_;
};

}  // namespace cachefly::command
