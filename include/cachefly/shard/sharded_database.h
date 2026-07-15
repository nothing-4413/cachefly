#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "cachefly/base/noncopyable.h"
#include "cachefly/command/database.h"
#include "cachefly/storage/kv_store.h"

namespace cachefly::shard {

class Shard final : public cachefly::NonCopyable {
public:
    using Task = std::function<void(storage::KvStore&)>;
    Shard(std::size_t maxmemory, storage::EvictionPolicy policy);
    ~Shard();
    void Post(Task task);

private:
    void Run();
    storage::KvStore store_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Task> tasks_;
    bool stopping_{false};
    std::thread worker_;
};

class ShardedDatabase final : public command::Database, public cachefly::NonCopyable {
public:
    explicit ShardedDatabase(
        std::size_t shard_count,
        std::size_t maxmemory = std::numeric_limits<std::size_t>::max(),
        storage::EvictionPolicy policy = storage::EvictionPolicy::kNoEviction);
    ~ShardedDatabase() override = default;

    [[nodiscard]] std::optional<std::string> Get(const std::string& key) override;
    [[nodiscard]] command::WriteResult Set(command::SetRequest request) override;
    [[nodiscard]] std::int64_t Delete(const std::vector<std::string>& keys) override;
    [[nodiscard]] std::int64_t Exists(const std::vector<std::string>& keys) override;
    [[nodiscard]] bool Expire(const std::string& key, std::chrono::milliseconds ttl) override;
    [[nodiscard]] std::int64_t TtlSeconds(const std::string& key) override;
    [[nodiscard]] command::IncrementResult Increment(const std::string& key,
                                                     std::int64_t delta) override;

    [[nodiscard]] std::size_t ShardForKey(const std::string& key) const;
    [[nodiscard]] std::size_t ShardCount() const noexcept;
    [[nodiscard]] std::vector<storage::SnapshotEntry> Snapshot();
    void Clear();

private:
    Shard& ForKey(const std::string& key);
    std::vector<std::unique_ptr<Shard>> shards_;
};

}  // namespace cachefly::shard
