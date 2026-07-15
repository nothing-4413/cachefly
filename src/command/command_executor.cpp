#include "cachefly/command/command_executor.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "cachefly/metrics/metrics.h"

namespace cachefly::command {
namespace {

std::string Uppercase(std::string text) {
    for (char& character : text) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return text;
}

bool ParseInteger(std::string_view text, std::int64_t* value) {
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), *value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

resp::Value ArityError(const std::string& command) {
    return resp::Value::Error("ERR wrong number of arguments for '" + command + "' command");
}

resp::Value IntegerError() {
    return resp::Value::Error("ERR value is not an integer or out of range");
}

resp::Value WriteReply(WriteResult result) {
    if (result == WriteResult::kOk) return resp::Value::Simple("OK");
    if (result == WriteResult::kConditionFailed) return resp::Value::Null();
    return resp::Value::Error("OOM command not allowed when used memory > maxmemory");
}

}  // namespace

CommandExecutor::CommandExecutor(Database* database, metrics::Metrics* metrics)
    : database_(database), metrics_(metrics) {
    if (database_ == nullptr) throw std::invalid_argument("database must not be null");
    RegisterCommands();
}

void CommandExecutor::SetMutationCallback(MutationCallback callback) {
    mutation_callback_ = std::move(callback);
}

resp::Value CommandExecutor::Execute(const std::vector<std::string>& arguments) const {
    const auto started = std::chrono::steady_clock::now();
    const auto finish = [this, started](resp::Value value) {
        if (metrics_ != nullptr) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started).count();
            metrics_->ObserveCommand(static_cast<std::uint64_t>(elapsed), value.type == resp::Type::kError);
        }
        return value;
    };
    if (arguments.empty()) return finish(resp::Value::Error("ERR empty command"));
    const std::string name = Uppercase(arguments.front());
    const CommandSpec* command = registry_.Find(name);
    if (command == nullptr) {
        return finish(resp::Value::Error("ERR unknown command '" + arguments.front() + "'"));
    }
    const std::size_t count = arguments.size();
    if (count < command->minimum_arguments ||
        (command->maximum_arguments.has_value() && count > *command->maximum_arguments)) {
        return finish(ArityError(command->name));
    }
    resp::Value response = command->handler(arguments);
    if (command->mutating && response.type != resp::Type::kError &&
        response.type != resp::Type::kNull && mutation_callback_) {
        mutation_callback_(arguments);
    }
    return finish(std::move(response));
}

const CommandRegistry& CommandExecutor::Registry() const noexcept { return registry_; }

void CommandExecutor::RegisterCommands() {
    registry_.Register({"PING", 1, 2, false, [](const auto& arguments) {
        return arguments.size() == 1 ? resp::Value::Simple("PONG")
                                     : resp::Value::Bulk(arguments[1]);
    }});
    registry_.Register({"ECHO", 2, 2, false, [](const auto& arguments) {
        return resp::Value::Bulk(arguments[1]);
    }});
    registry_.Register({"GET", 2, 2, false, [this](const auto& arguments) {
        const auto value = database_->Get(arguments[1]);
        return value.has_value() ? resp::Value::Bulk(*value) : resp::Value::Null();
    }});
    registry_.Register({"SET", 3, std::nullopt, true, [this](const auto& arguments) {
        return Set(arguments);
    }});
    registry_.Register({"DEL", 2, std::nullopt, true, [this](const auto& arguments) {
        return Del(arguments);
    }});
    registry_.Register({"EXISTS", 2, std::nullopt, false, [this](const auto& arguments) {
        return Exists(arguments);
    }});
    registry_.Register({"EXPIRE", 3, 3, true, [this](const auto& arguments) {
        std::int64_t seconds = 0;
        if (!ParseInteger(arguments[2], &seconds) ||
            seconds > std::numeric_limits<std::int64_t>::max() / 1000) return IntegerError();
        const auto ttl = seconds <= 0 ? std::chrono::milliseconds(0)
                                      : std::chrono::milliseconds(seconds * 1000);
        return resp::Value::Integer(database_->Expire(
            arguments[1], ttl) ? 1 : 0);
    }});
    registry_.Register({"TTL", 2, 2, false, [this](const auto& arguments) {
        return resp::Value::Integer(database_->TtlSeconds(arguments[1]));
    }});
    registry_.Register({"MGET", 2, std::nullopt, false, [this](const auto& arguments) {
        return MGet(arguments);
    }});
    registry_.Register({"MSET", 3, std::nullopt, true, [this](const auto& arguments) {
        return MSet(arguments);
    }});
    registry_.Register({"INCR", 2, 2, true, [this](const auto& arguments) {
        const IncrementResult result = database_->Increment(arguments[1], 1);
        if (result.status == IncrementStatus::kOk) return resp::Value::Integer(result.value);
        if (result.status == IncrementStatus::kNoMemory) return resp::Value::Error("OOM increment failed");
        return IntegerError();
    }});
    registry_.Register({"DECR", 2, 2, true, [this](const auto& arguments) {
        const IncrementResult result = database_->Increment(arguments[1], -1);
        if (result.status == IncrementStatus::kOk) return resp::Value::Integer(result.value);
        if (result.status == IncrementStatus::kNoMemory) return resp::Value::Error("OOM decrement failed");
        return IntegerError();
    }});
}

resp::Value CommandExecutor::Set(const std::vector<std::string>& arguments) const {
    SetRequest request{arguments[1], arguments[2], std::nullopt, SetCondition::kNone};
    for (std::size_t index = 3; index < arguments.size();) {
        const std::string option = Uppercase(arguments[index]);
        if ((option == "EX" || option == "PX") && index + 1 < arguments.size() &&
            !request.ttl.has_value()) {
            std::int64_t amount = 0;
            if (!ParseInteger(arguments[index + 1], &amount) || amount <= 0 ||
                (option == "EX" && amount > std::numeric_limits<std::int64_t>::max() / 1000)) {
                return IntegerError();
            }
            request.ttl = std::chrono::milliseconds(option == "EX" ? amount * 1000 : amount);
            index += 2;
        } else if (option == "NX" && request.condition == SetCondition::kNone) {
            request.condition = SetCondition::kIfAbsent;
            ++index;
        } else if (option == "XX" && request.condition == SetCondition::kNone) {
            request.condition = SetCondition::kIfPresent;
            ++index;
        } else {
            return resp::Value::Error("ERR syntax error");
        }
    }
    return WriteReply(database_->Set(std::move(request)));
}

resp::Value CommandExecutor::Del(const std::vector<std::string>& arguments) const {
    return resp::Value::Integer(database_->Delete(
        std::vector<std::string>(arguments.begin() + 1, arguments.end())));
}

resp::Value CommandExecutor::Exists(const std::vector<std::string>& arguments) const {
    return resp::Value::Integer(database_->Exists(
        std::vector<std::string>(arguments.begin() + 1, arguments.end())));
}

resp::Value CommandExecutor::MGet(const std::vector<std::string>& arguments) const {
    std::vector<resp::Value> values;
    values.reserve(arguments.size() - 1);
    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const auto value = database_->Get(arguments[index]);
        values.push_back(value.has_value() ? resp::Value::Bulk(*value) : resp::Value::Null());
    }
    return resp::Value::Array(std::move(values));
}

resp::Value CommandExecutor::MSet(const std::vector<std::string>& arguments) const {
    if (arguments.size() % 2 == 0) return ArityError("mset");
    for (std::size_t index = 1; index < arguments.size(); index += 2) {
        const WriteResult result = database_->Set(
            {arguments[index], arguments[index + 1], std::nullopt, SetCondition::kNone});
        if (result != WriteResult::kOk) return WriteReply(result);
    }
    return resp::Value::Simple("OK");
}

}  // namespace cachefly::command
