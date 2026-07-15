#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace cachefly::command {

enum class SetCondition { kNone, kIfAbsent, kIfPresent };
enum class WriteResult { kOk, kConditionFailed, kNoMemory };
enum class IncrementStatus { kOk, kNotInteger, kOverflow, kNoMemory };

struct SetRequest {
    std::string key;
    std::string value;
    std::optional<std::chrono::milliseconds> ttl;
    SetCondition condition{SetCondition::kNone};
};

struct IncrementResult {
    IncrementStatus status{IncrementStatus::kOk};
    std::int64_t value{0};
};

class Database {
public:
    using KeyValue = std::pair<std::string, std::string>;
    virtual ~Database() = default;

    [[nodiscard]] virtual std::optional<std::string> Get(const std::string& key) = 0;
    [[nodiscard]] virtual WriteResult Set(SetRequest request) = 0;
    [[nodiscard]] virtual WriteResult MSet(std::vector<KeyValue> values) = 0;
    [[nodiscard]] virtual std::int64_t Delete(const std::vector<std::string>& keys) = 0;
    [[nodiscard]] virtual std::int64_t Exists(const std::vector<std::string>& keys) = 0;
    [[nodiscard]] virtual bool Expire(const std::string& key,
                                      std::chrono::milliseconds ttl) = 0;
    [[nodiscard]] virtual std::int64_t TtlSeconds(const std::string& key) = 0;
    [[nodiscard]] virtual IncrementResult Increment(const std::string& key,
                                                    std::int64_t delta) = 0;
};

}  // namespace cachefly::command
