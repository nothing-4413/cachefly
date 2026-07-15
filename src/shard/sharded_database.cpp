#include "cachefly/shard/sharded_database.h"

#include <chrono>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "cachefly/base/logger.h"

namespace cachefly::shard {

Shard::Shard(std::size_t maxmemory, storage::EvictionPolicy policy,
             metrics::Metrics* metrics)
    : store_(storage::KvStore::Clock::now, maxmemory, policy, metrics),
      worker_([this] { Run(); }) {}

Shard::~Shard() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_one();
    if (worker_.joinable()) worker_.join();
}

void Shard::Post(Task task) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) throw std::runtime_error("shard is stopping");
        tasks_.push_back(std::move(task));
    }
    condition_.notify_one();
}

void Shard::Run() {
    while (true) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            condition_.wait_for(lock, std::chrono::milliseconds(100),
                                [this] { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) break;
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
        }
        try {
            if (task) task(store_);
            store_.ActiveExpire(64);
        } catch (const std::exception& error) {
            LOG_ERROR("shard task failed: " + std::string(error.what()));
        }
    }
}

ShardedDatabase::ShardedDatabase(std::size_t shard_count,
                                 std::size_t maxmemory,
                                 storage::EvictionPolicy policy,
                                 metrics::Metrics* metrics) {
    if (shard_count == 0) throw std::invalid_argument("shard count must be positive");
    shards_.reserve(shard_count);
    for (std::size_t index = 0; index < shard_count; ++index) {
        const std::size_t budget = maxmemory / shard_count +
                                   (index < maxmemory % shard_count ? 1 : 0);
        shards_.push_back(std::make_unique<Shard>(budget, policy, metrics));
    }
}

std::optional<std::string> ShardedDatabase::Get(const std::string& key) {
    std::shared_lock lock(transaction_mutex_);
    return ForKey(key).Submit([key](storage::KvStore& store) { return store.Get(key); }).get();
}

command::WriteResult ShardedDatabase::Set(command::SetRequest request) {
    std::shared_lock lock(transaction_mutex_);
    const std::string key = request.key;
    return ForKey(key).Submit([request = std::move(request)](storage::KvStore& store) mutable {
        return store.Set(std::move(request));
    }).get();
}

command::WriteResult ShardedDatabase::MSet(
    std::vector<command::Database::KeyValue> values) {
    std::unique_lock lock(transaction_mutex_);
    std::vector<std::vector<command::Database::KeyValue>> groups(shards_.size());
    for (auto& [key, value] : values) {
        groups[ShardForKey(key)].emplace_back(std::move(key), std::move(value));
    }

    std::vector<std::optional<storage::KvStore>> checkpoints(shards_.size());
    std::vector<std::size_t> touched;
    for (std::size_t index = 0; index < groups.size(); ++index) {
        if (groups[index].empty()) continue;
        touched.push_back(index);
        checkpoints[index] = shards_[index]->Submit(
            [](storage::KvStore& store) { return store; }).get();
    }

    const auto rollback = [this, &checkpoints, &touched] {
        std::vector<std::future<void>> futures;
        for (const std::size_t index : touched) {
            futures.push_back(shards_[index]->Submit(
                [checkpoint = std::move(*checkpoints[index])](storage::KvStore& store) mutable {
                    store = std::move(checkpoint);
                }));
        }
        for (auto& future : futures) future.get();
    };

    try {
        for (const std::size_t index : touched) {
            const command::WriteResult result = shards_[index]->Submit(
                [entries = std::move(groups[index])](storage::KvStore& store) mutable {
                    return store.MSet(std::move(entries));
                }).get();
            if (result != command::WriteResult::kOk) {
                rollback();
                return result;
            }
        }
    } catch (...) {
        rollback();
        throw;
    }
    return command::WriteResult::kOk;
}

std::int64_t ShardedDatabase::Delete(const std::vector<std::string>& keys) {
    std::shared_lock lock(transaction_mutex_);
    std::int64_t total = 0;
    for (const std::string& key : keys) {
        total += ForKey(key).Submit([key](storage::KvStore& store) {
            return store.Delete({key});
        }).get();
    }
    return total;
}

std::int64_t ShardedDatabase::Exists(const std::vector<std::string>& keys) {
    std::shared_lock lock(transaction_mutex_);
    std::int64_t total = 0;
    for (const std::string& key : keys) {
        total += ForKey(key).Submit([key](storage::KvStore& store) {
            return store.Exists({key});
        }).get();
    }
    return total;
}

bool ShardedDatabase::Expire(const std::string& key, std::chrono::milliseconds ttl) {
    std::shared_lock lock(transaction_mutex_);
    return ForKey(key).Submit([key, ttl](storage::KvStore& store) {
        return store.Expire(key, ttl);
    }).get();
}

std::int64_t ShardedDatabase::TtlSeconds(const std::string& key) {
    std::shared_lock lock(transaction_mutex_);
    return ForKey(key).Submit([key](storage::KvStore& store) {
        return store.TtlSeconds(key);
    }).get();
}

command::IncrementResult ShardedDatabase::Increment(const std::string& key, std::int64_t delta) {
    std::shared_lock lock(transaction_mutex_);
    return ForKey(key).Submit([key, delta](storage::KvStore& store) {
        return store.Increment(key, delta);
    }).get();
}

std::size_t ShardedDatabase::ShardForKey(const std::string& key) const {
    return std::hash<std::string>{}(key) % shards_.size();
}

std::size_t ShardedDatabase::ShardCount() const noexcept { return shards_.size(); }

std::vector<storage::SnapshotEntry> ShardedDatabase::Snapshot() {
    std::unique_lock lock(transaction_mutex_);
    std::vector<std::future<std::vector<storage::SnapshotEntry>>> futures;
    futures.reserve(shards_.size());
    for (auto& shard : shards_) {
        futures.push_back(shard->Submit(
            [](storage::KvStore& store) { return store.Snapshot(); }));
    }
    std::vector<storage::SnapshotEntry> flattened;
    for (auto& future : futures) {
        auto shard_entries = future.get();
        flattened.insert(flattened.end(),
                         std::make_move_iterator(shard_entries.begin()),
                         std::make_move_iterator(shard_entries.end()));
    }
    return flattened;
}

std::pair<std::size_t, std::size_t> ShardedDatabase::Stats() {
    std::shared_lock lock(transaction_mutex_);
    std::vector<std::future<std::pair<std::size_t, std::size_t>>> futures;
    futures.reserve(shards_.size());
    for (auto& shard : shards_) {
        futures.push_back(shard->Submit([](storage::KvStore& store) {
            return std::pair{store.Size(), store.MemoryUsage()};
        }));
    }
    std::size_t keys = 0;
    std::size_t bytes = 0;
    for (auto& future : futures) {
        const auto [shard_keys, shard_bytes] = future.get();
        keys += shard_keys;
        bytes += shard_bytes;
    }
    return {keys, bytes};
}

void ShardedDatabase::Clear() {
    std::unique_lock lock(transaction_mutex_);
    std::vector<std::future<void>> futures;
    futures.reserve(shards_.size());
    for (auto& shard : shards_) {
        futures.push_back(shard->Submit([](storage::KvStore& store) {
            store.Clear();
        }));
    }
    for (auto& future : futures) future.get();
}

Shard& ShardedDatabase::ForKey(const std::string& key) { return *shards_[ShardForKey(key)]; }

}  // namespace cachefly::shard
