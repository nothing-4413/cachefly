#include "cachefly/command/command_registry.h"

#include <stdexcept>
#include <utility>

namespace cachefly::command {

void CommandRegistry::Register(CommandSpec command) {
    const std::string name = command.name;
    const auto [iterator, inserted] = commands_.emplace(name, std::move(command));
    static_cast<void>(iterator);
    if (!inserted) throw std::logic_error("duplicate command registration: " + name);
}

const CommandSpec* CommandRegistry::Find(const std::string& uppercase_name) const {
    const auto found = commands_.find(uppercase_name);
    return found == commands_.end() ? nullptr : &found->second;
}

std::size_t CommandRegistry::Size() const noexcept { return commands_.size(); }

}  // namespace cachefly::command
