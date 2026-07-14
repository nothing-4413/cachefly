#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cachefly/command/database.h"

namespace cachefly::storage {

class KvStore final : public command::Database {
public:
    using Clock = std::chrono::steady_clock;
    using ClockFunction = std::function<Clock::time_point()>;

    explicit KvStore(ClockFunction clock = Clock::now);

    [[nodiscard]] std::optional<std::string> Get(const std::string& key) override;
    [[nodiscard]] command::WriteResult Set(command::SetRequest request) override;
    [[nodiscard]] std::int64_t Delete(const std::vector<std::string>& keys) override;
    [[nodiscard]] std::int64_t Exists(const std::vector<std::string>& keys) override;
    [[nodiscard]] bool Expire(const std::string& key,
                              std::chrono::milliseconds ttl) override;
    [[nodiscard]] std::int64_t TtlSeconds(const std::string& key) override;
    [[nodiscard]] command::IncrementResult Increment(const std::string& key,
                                                     std::int64_t delta) override;

    // Examines at most max_samples keys and returns the number removed.
    std::size_t ActiveExpire(std::size_t max_samples);
    [[nodiscard]] std::size_t Size() const noexcept;
    void Clear() noexcept;

private:
    struct Entry {
        std::string value;
        std::optional<Clock::time_point> expires_at;
    };

    using Map = std::unordered_map<std::string, Entry>;

    [[nodiscard]] bool IsExpired(const Entry& entry, Clock::time_point now) const;
    bool RemoveIfExpired(Map::iterator iterator, Clock::time_point now);

    ClockFunction clock_;
    Map entries_;
    std::size_t expire_scan_offset_{0};
};

}  // namespace cachefly::storage
