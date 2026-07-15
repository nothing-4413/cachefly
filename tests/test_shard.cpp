#include <atomic>
#include <string>
#include <thread>
#include <stdexcept>
#include <vector>

#include "cachefly/command/database.h"
#include "cachefly/shard/sharded_database.h"
#include "test_harness.h"

TEST_CASE("same key always maps to same shard") {
    cachefly::shard::ShardedDatabase database(4);
    EXPECT_EQ(database.ShardForKey("alpha"), database.ShardForKey("alpha"));
    EXPECT_TRUE(database.ShardForKey("alpha") < database.ShardCount());
}

TEST_CASE("sharded database preserves command semantics") {
    cachefly::shard::ShardedDatabase database(4);
    EXPECT_EQ(database.Set({"a", "1", std::nullopt,
                            cachefly::command::SetCondition::kNone}),
              cachefly::command::WriteResult::kOk);
    EXPECT_EQ(database.Set({"b", "2", std::nullopt,
                            cachefly::command::SetCondition::kNone}),
              cachefly::command::WriteResult::kOk);
    EXPECT_EQ(database.Exists({"a", "b", "none"}), 2);
    EXPECT_EQ(database.Delete({"a", "b"}), 2);
}

TEST_CASE("concurrent increments are serialized by owning shard") {
    cachefly::shard::ShardedDatabase database(4);
    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([&database] {
            for (int count = 0; count < 100; ++count) {
                static_cast<void>(database.Increment("counter", 1));
            }
        });
    }
    for (auto& thread : threads) thread.join();
    EXPECT_EQ(database.Get("counter"), std::optional<std::string>("800"));
}

TEST_CASE("shard task exceptions propagate through futures") {
    cachefly::shard::Shard shard(
        1024, cachefly::storage::EvictionPolicy::kNoEviction, nullptr);
    auto future = shard.Submit([](cachefly::storage::KvStore&) -> int {
        throw std::runtime_error("task failed");
    });
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_CASE("cross shard MSET rolls back every touched shard on OOM") {
    cachefly::storage::KvStore probe;
    static_cast<void>(probe.Set({"aa", "1", std::nullopt,
                                 cachefly::command::SetCondition::kNone}));
    cachefly::shard::ShardedDatabase database(
        2, probe.MemoryUsage() * 2, cachefly::storage::EvictionPolicy::kNoEviction);
    std::vector<std::string> shard_zero;
    std::vector<std::string> shard_one;
    for (char suffix = 'a'; suffix <= 'z'; ++suffix) {
        std::string key{"k"};
        key.push_back(suffix);
        (database.ShardForKey(key) == 0 ? shard_zero : shard_one).push_back(key);
    }
    EXPECT_TRUE(!shard_zero.empty());
    EXPECT_TRUE(shard_one.size() >= 2);
    EXPECT_EQ(database.MSet({{shard_zero[0], "1"},
                             {shard_one[0], "1"},
                             {shard_one[1], "1"}}),
              cachefly::command::WriteResult::kNoMemory);
    EXPECT_TRUE(!database.Get(shard_zero[0]).has_value());
    EXPECT_TRUE(!database.Get(shard_one[0]).has_value());
    EXPECT_TRUE(!database.Get(shard_one[1]).has_value());
}
