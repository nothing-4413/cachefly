#include "cachefly/shard/sharded_database.h"

#include <chrono>
#include <future>
#include <atomic>
#include <iterator>
#include <stdexcept>
#include <utility>

#include "cachefly/base/logger.h"

namespace cachefly::shard {

Shard::Shard(std::size_t maxmemory, storage::EvictionPolicy policy)
    : store_(storage::KvStore::Clock::now, maxmemory, policy), worker_([this] { Run(); }) {}

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
                                 storage::EvictionPolicy policy) {
    if (shard_count == 0) throw std::invalid_argument("shard count must be positive");
    shards_.reserve(shard_count);
    for (std::size_t index = 0; index < shard_count; ++index) {
        const std::size_t budget = maxmemory / shard_count +
                                   (index < maxmemory % shard_count ? 1 : 0);
        shards_.push_back(std::make_unique<Shard>(budget, policy));
    }
}

std::optional<std::string> ShardedDatabase::Get(const std::string& key) {
    auto promise = std::make_shared<std::promise<std::optional<std::string>>>();
    auto future = promise->get_future();
    ForKey(key).Post([key, promise](storage::KvStore& store) { promise->set_value(store.Get(key)); });
    return future.get();
}

command::WriteResult ShardedDatabase::Set(command::SetRequest request) {
    auto promise = std::make_shared<std::promise<command::WriteResult>>();
    auto future = promise->get_future();
    const std::string key = request.key;
    ForKey(key).Post([request = std::move(request), promise](storage::KvStore& store) mutable {
        promise->set_value(store.Set(std::move(request)));
    });
    return future.get();
}

std::int64_t ShardedDatabase::Delete(const std::vector<std::string>& keys) {
    std::int64_t total = 0;
    for (const std::string& key : keys) {
        auto promise = std::make_shared<std::promise<std::int64_t>>();
        auto future = promise->get_future();
        ForKey(key).Post([key, promise](storage::KvStore& store) {
            promise->set_value(store.Delete({key}));
        });
        total += future.get();
    }
    return total;
}

std::int64_t ShardedDatabase::Exists(const std::vector<std::string>& keys) {
    std::int64_t total = 0;
    for (const std::string& key : keys) {
        auto promise = std::make_shared<std::promise<std::int64_t>>();
        auto future = promise->get_future();
        ForKey(key).Post([key, promise](storage::KvStore& store) {
            promise->set_value(store.Exists({key}));
        });
        total += future.get();
    }
    return total;
}

bool ShardedDatabase::Expire(const std::string& key, std::chrono::milliseconds ttl) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    ForKey(key).Post([key, ttl, promise](storage::KvStore& store) {
        promise->set_value(store.Expire(key, ttl));
    });
    return future.get();
}

std::int64_t ShardedDatabase::TtlSeconds(const std::string& key) {
    auto promise = std::make_shared<std::promise<std::int64_t>>();
    auto future = promise->get_future();
    ForKey(key).Post([key, promise](storage::KvStore& store) {
        promise->set_value(store.TtlSeconds(key));
    });
    return future.get();
}

command::IncrementResult ShardedDatabase::Increment(const std::string& key, std::int64_t delta) {
    auto promise = std::make_shared<std::promise<command::IncrementResult>>();
    auto future = promise->get_future();
    ForKey(key).Post([key, delta, promise](storage::KvStore& store) {
        promise->set_value(store.Increment(key, delta));
    });
    return future.get();
}

std::size_t ShardedDatabase::ShardForKey(const std::string& key) const {
    return std::hash<std::string>{}(key) % shards_.size();
}

std::size_t ShardedDatabase::ShardCount() const noexcept { return shards_.size(); }

std::vector<storage::SnapshotEntry> ShardedDatabase::Snapshot() {
    auto remaining = std::make_shared<std::atomic<std::size_t>>(shards_.size());
    auto results = std::make_shared<std::vector<std::vector<storage::SnapshotEntry>>>(shards_.size());
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();
    for (std::size_t index = 0; index < shards_.size(); ++index) {
        shards_[index]->Post([index, remaining, results, promise](storage::KvStore& store) {
            (*results)[index] = store.Snapshot();
            if (remaining->fetch_sub(1, std::memory_order_acq_rel) == 1) promise->set_value();
        });
    }
    future.get();
    std::vector<storage::SnapshotEntry> flattened;
    for (auto& shard_entries : *results) {
        flattened.insert(flattened.end(),
                         std::make_move_iterator(shard_entries.begin()),
                         std::make_move_iterator(shard_entries.end()));
    }
    return flattened;
}

void ShardedDatabase::Clear() {
    auto remaining = std::make_shared<std::atomic<std::size_t>>(shards_.size());
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();
    for (auto& shard : shards_) {
        shard->Post([remaining, promise](storage::KvStore& store) {
            store.Clear();
            if (remaining->fetch_sub(1, std::memory_order_acq_rel) == 1) promise->set_value();
        });
    }
    future.get();
}

Shard& ShardedDatabase::ForKey(const std::string& key) { return *shards_[ShardForKey(key)]; }

}  // namespace cachefly::shard
