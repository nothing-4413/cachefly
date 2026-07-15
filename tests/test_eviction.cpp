#include <optional>
#include <string>
#include <utility>

#include "cachefly/storage/kv_store.h"
#include "test_harness.h"

namespace {
cachefly::command::WriteResult Put(cachefly::storage::KvStore& store,
                                   std::string key, std::string value) {
    return store.Set({std::move(key), std::move(value), std::nullopt,
                      cachefly::command::SetCondition::kNone});
}
}  // namespace

TEST_CASE("noeviction rejects writes and keeps the old value") {
    cachefly::storage::KvStore probe;
    static_cast<void>(Put(probe, "a", "1"));
    cachefly::storage::KvStore store(cachefly::storage::KvStore::Clock::now,
        probe.MemoryUsage(), cachefly::storage::EvictionPolicy::kNoEviction);
    EXPECT_EQ(Put(store, "a", "1"), cachefly::command::WriteResult::kOk);
    EXPECT_EQ(Put(store, "b", "2"), cachefly::command::WriteResult::kNoMemory);
    EXPECT_EQ(store.Get("a"), std::optional<std::string>("1"));
}

TEST_CASE("LRU evicts the least recently used key") {
    cachefly::storage::KvStore probe;
    static_cast<void>(Put(probe, "a", "1"));
    cachefly::storage::KvStore store(cachefly::storage::KvStore::Clock::now,
        probe.MemoryUsage() * 2, cachefly::storage::EvictionPolicy::kLru);
    static_cast<void>(Put(store, "a", "1"));
    static_cast<void>(Put(store, "b", "2"));
    static_cast<void>(store.Get("a"));
    EXPECT_EQ(Put(store, "c", "3"), cachefly::command::WriteResult::kOk);
    EXPECT_TRUE(store.Get("a").has_value());
    EXPECT_TRUE(!store.Get("b").has_value());
}

TEST_CASE("LFU retains a frequently accessed key") {
    cachefly::storage::KvStore probe;
    static_cast<void>(Put(probe, "a", "1"));
    cachefly::storage::KvStore store(cachefly::storage::KvStore::Clock::now,
        probe.MemoryUsage() * 2, cachefly::storage::EvictionPolicy::kLfu);
    static_cast<void>(Put(store, "a", "1"));
    static_cast<void>(Put(store, "b", "2"));
    static_cast<void>(store.Get("a"));
    static_cast<void>(store.Get("a"));
    static_cast<void>(Put(store, "c", "3"));
    EXPECT_TRUE(store.Get("a").has_value());
    EXPECT_TRUE(!store.Get("b").has_value());
}

TEST_CASE("MSET rejects an oversized batch without partial writes") {
    cachefly::storage::KvStore probe;
    static_cast<void>(Put(probe, "aa", "old"));
    cachefly::storage::KvStore store(cachefly::storage::KvStore::Clock::now,
        probe.MemoryUsage(), cachefly::storage::EvictionPolicy::kNoEviction);
    EXPECT_EQ(Put(store, "aa", "old"), cachefly::command::WriteResult::kOk);
    EXPECT_EQ(store.MSet({{"aa", "new"}, {"bb", "2"}}),
              cachefly::command::WriteResult::kNoMemory);
    EXPECT_EQ(store.Get("aa"), std::optional<std::string>("old"));
    EXPECT_TRUE(!store.Get("bb").has_value());
}
