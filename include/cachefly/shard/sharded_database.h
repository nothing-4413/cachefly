#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "cachefly/base/noncopyable.h"
#include "cachefly/command/database.h"
#include "cachefly/storage/kv_store.h"

namespace cachefly::metrics { class Metrics; }

namespace cachefly::shard {

class Shard final : public cachefly::NonCopyable {
public:
    using Task = std::function<void(storage::KvStore&)>;
    Shard(std::size_t maxmemory, storage::EvictionPolicy policy,
          metrics::Metrics* metrics);
    ~Shard();
    void Post(Task task);

    template <typename Function>
    auto Submit(Function&& function)
        -> std::future<std::invoke_result_t<Function, storage::KvStore&>> {
        using Result = std::invoke_result_t<Function, storage::KvStore&>;
        auto task = std::make_shared<std::packaged_task<Result(storage::KvStore&)>>(
            std::forward<Function>(function));
        auto future = task->get_future();
        Post([task](storage::KvStore& store) { (*task)(store); });
        return future;
    }

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
        storage::EvictionPolicy policy = storage::EvictionPolicy::kNoEviction,
        metrics::Metrics* metrics = nullptr);
    ~ShardedDatabase() override = default;

    [[nodiscard]] std::optional<std::string> Get(const std::string& key) override;
    [[nodiscard]] command::WriteResult Set(command::SetRequest request) override;
    [[nodiscard]] command::WriteResult MSet(
        std::vector<command::Database::KeyValue> values) override;
    [[nodiscard]] std::int64_t Delete(const std::vector<std::string>& keys) override;
    [[nodiscard]] std::int64_t Exists(const std::vector<std::string>& keys) override;
    [[nodiscard]] bool Expire(const std::string& key, std::chrono::milliseconds ttl) override;
    [[nodiscard]] std::int64_t TtlSeconds(const std::string& key) override;
    [[nodiscard]] command::IncrementResult Increment(const std::string& key,
                                                     std::int64_t delta) override;

    [[nodiscard]] std::size_t ShardForKey(const std::string& key) const;
    [[nodiscard]] std::size_t ShardCount() const noexcept;
    [[nodiscard]] std::vector<storage::SnapshotEntry> Snapshot();
    [[nodiscard]] std::pair<std::size_t, std::size_t> Stats();
    void Clear();

private:
    Shard& ForKey(const std::string& key);
    std::vector<std::unique_ptr<Shard>> shards_;
    mutable std::shared_mutex transaction_mutex_;
};

}  // namespace cachefly::shard
