#pragma once

#include <string>
#include <functional>
#include <vector>

#include "cachefly/base/noncopyable.h"
#include "cachefly/command/command_registry.h"
#include "cachefly/command/database.h"

namespace cachefly::command {

class CommandExecutor final : public cachefly::NonCopyable {
public:
    using MutationCallback = std::function<void(const std::vector<std::string>&)>;

    explicit CommandExecutor(Database* database);

    void SetMutationCallback(MutationCallback callback);
    [[nodiscard]] resp::Value Execute(const std::vector<std::string>& arguments) const;
    [[nodiscard]] const CommandRegistry& Registry() const noexcept;

private:
    void RegisterCommands();
    [[nodiscard]] resp::Value Set(const std::vector<std::string>& arguments) const;
    [[nodiscard]] resp::Value Del(const std::vector<std::string>& arguments) const;
    [[nodiscard]] resp::Value Exists(const std::vector<std::string>& arguments) const;
    [[nodiscard]] resp::Value MGet(const std::vector<std::string>& arguments) const;
    [[nodiscard]] resp::Value MSet(const std::vector<std::string>& arguments) const;

    Database* database_;
    CommandRegistry registry_;
    MutationCallback mutation_callback_;
};

}  // namespace cachefly::command
