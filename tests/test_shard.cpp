#include <atomic>
#include <string>
#include <thread>
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
