#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "cachefly/command/database.h"
#include "cachefly/storage/kv_store.h"
#include "test_harness.h"

TEST_CASE("KV store set get delete and conditions") {
    cachefly::storage::KvStore store;
    EXPECT_EQ(store.Set({"key", "value", std::nullopt,
                         cachefly::command::SetCondition::kNone}),
              cachefly::command::WriteResult::kOk);
    EXPECT_EQ(store.Get("key"), std::optional<std::string>("value"));
    EXPECT_EQ(store.Set({"key", "other", std::nullopt,
                         cachefly::command::SetCondition::kIfAbsent}),
              cachefly::command::WriteResult::kConditionFailed);
    EXPECT_EQ(store.Exists({"key", "missing"}), 1);
    EXPECT_EQ(store.Delete({"key"}), 1);
    EXPECT_TRUE(!store.Get("key").has_value());
}

TEST_CASE("TTL supports lazy expiration and Redis return values") {
    using namespace std::chrono_literals;
    auto now = cachefly::storage::KvStore::Clock::now();
    cachefly::storage::KvStore store([&now] { return now; });
    EXPECT_EQ(store.Set({"key", "value", 2500ms,
                         cachefly::command::SetCondition::kNone}),
              cachefly::command::WriteResult::kOk);
    EXPECT_EQ(store.TtlSeconds("key"), 2);
    now += 3s;
    EXPECT_EQ(store.TtlSeconds("key"), -2);
    EXPECT_TRUE(!store.Get("key").has_value());
}

TEST_CASE("active expiration has a bounded scan budget") {
    using namespace std::chrono_literals;
    auto now = cachefly::storage::KvStore::Clock::now();
    cachefly::storage::KvStore store([&now] { return now; });
    for (int index = 0; index < 10; ++index) {
        EXPECT_EQ(store.Set({std::to_string(index), "v", 1ms,
                             cachefly::command::SetCondition::kNone}),
                  cachefly::command::WriteResult::kOk);
    }
    now += 2ms;
    EXPECT_TRUE(store.ActiveExpire(3) <= 3U);
    while (store.ActiveExpire(10) != 0) {}
    EXPECT_EQ(store.Size(), 0U);
}

TEST_CASE("increment preserves integer semantics") {
    cachefly::storage::KvStore store;
    EXPECT_EQ(store.Increment("counter", 1).value, 1);
    EXPECT_EQ(store.Increment("counter", -1).value, 0);
    EXPECT_EQ(store.Set({"text", "abc", std::nullopt,
                         cachefly::command::SetCondition::kNone}),
              cachefly::command::WriteResult::kOk);
    EXPECT_EQ(store.Increment("text", 1).status,
              cachefly::command::IncrementStatus::kNotInteger);
}
