#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cachefly/command/database.h"

namespace cachefly::storage {

enum class EvictionPolicy { kLru, kLfu, kRandom, kNoEviction };
[[nodiscard]] EvictionPolicy ParseEvictionPolicy(const std::string& name);

class KvStore final : public command::Database {
public:
    using Clock = std::chrono::steady_clock;
    using ClockFunction = std::function<Clock::time_point()>;

    explicit KvStore(ClockFunction clock = Clock::now,
                     std::size_t maxmemory = std::numeric_limits<std::size_t>::max(),
                     EvictionPolicy policy = EvictionPolicy::kNoEviction);

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
    [[nodiscard]] std::size_t MemoryUsage() const noexcept;
    void Clear() noexcept;

private:
    struct Entry {
        std::string value;
        std::optional<Clock::time_point> expires_at;
        std::uint64_t last_access{0};
        std::uint64_t frequency{1};
    };

    using Map = std::unordered_map<std::string, Entry>;

    [[nodiscard]] bool IsExpired(const Entry& entry, Clock::time_point now) const;
    bool RemoveIfExpired(Map::iterator iterator, Clock::time_point now);
    [[nodiscard]] std::size_t EntryBytes(const std::string& key,
                                         const std::string& value) const noexcept;
    [[nodiscard]] Map::iterator SelectVictim(const std::string& protected_key);
    bool MakeRoom(std::size_t projected, const std::string& protected_key);

    ClockFunction clock_;
    std::size_t maxmemory_;
    EvictionPolicy policy_;
    Map entries_;
    std::size_t memory_usage_{0};
    std::uint64_t access_clock_{0};
    std::uint64_t random_state_{0x9e3779b97f4a7c15ULL};
    std::size_t expire_scan_offset_{0};
};

}  // namespace cachefly::storage
