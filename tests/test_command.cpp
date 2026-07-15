#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "cachefly/command/command_executor.h"
#include "cachefly/command/async_dispatcher.h"
#include "test_harness.h"

namespace {

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

class FakeDatabase final : public cachefly::command::Database {
public:
    std::optional<std::string> Get(const std::string& key) override {
        const auto found = values.find(key);
        return found == values.end() ? std::nullopt : std::optional(found->second);
    }

    cachefly::command::WriteResult Set(cachefly::command::SetRequest request) override {
        const bool exists = values.find(request.key) != values.end();
        if (request.condition == cachefly::command::SetCondition::kIfAbsent && exists) {
            return cachefly::command::WriteResult::kConditionFailed;
        }
        if (request.condition == cachefly::command::SetCondition::kIfPresent && !exists) {
            return cachefly::command::WriteResult::kConditionFailed;
        }
        values[request.key] = request.value;
        return cachefly::command::WriteResult::kOk;
    }

    cachefly::command::WriteResult MSet(
        std::vector<cachefly::command::Database::KeyValue> entries) override {
        if (fail_mset) return cachefly::command::WriteResult::kNoMemory;
        auto replacement = values;
        for (auto& [key, value] : entries) replacement[std::move(key)] = std::move(value);
        values = std::move(replacement);
        return cachefly::command::WriteResult::kOk;
    }

    std::int64_t Delete(const std::vector<std::string>& keys) override {
        std::int64_t count = 0;
        for (const auto& key : keys) count += static_cast<std::int64_t>(values.erase(key));
        return count;
    }

    std::int64_t Exists(const std::vector<std::string>& keys) override {
        std::int64_t count = 0;
        for (const auto& key : keys) if (values.find(key) != values.end()) ++count;
        return count;
    }

    bool Expire(const std::string& key, std::chrono::milliseconds ttl) override {
        static_cast<void>(ttl);
        return values.find(key) != values.end();
    }

    std::int64_t TtlSeconds(const std::string& key) override {
        return values.find(key) != values.end() ? -1 : -2;
    }

    cachefly::command::IncrementResult Increment(const std::string& key,
                                                 std::int64_t delta) override {
        std::int64_t current = 0;
        if (values.find(key) != values.end()) {
            const std::string& text = values[key];
            const auto result = std::from_chars(text.data(), text.data() + text.size(), current);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
                return {cachefly::command::IncrementStatus::kNotInteger, 0};
            }
        }
        if ((delta > 0 && current == std::numeric_limits<std::int64_t>::max()) ||
            (delta < 0 && current == std::numeric_limits<std::int64_t>::min())) {
            return {cachefly::command::IncrementStatus::kOverflow, 0};
        }
        current += delta;
        values[key] = std::to_string(current);
        return {cachefly::command::IncrementStatus::kOk, current};
    }

    std::unordered_map<std::string, std::string> values;
    bool fail_mset{false};
};

}  // namespace

TEST_CASE("command registry exposes supported commands") {
    FakeDatabase database;
    cachefly::command::CommandExecutor executor(&database);
    EXPECT_EQ(executor.Registry().Size(), 12U);
    EXPECT_TRUE(executor.Registry().Find("SET")->mutating);
}

TEST_CASE("basic Redis commands return RESP values") {
    FakeDatabase database;
    cachefly::command::CommandExecutor executor(&database);
    EXPECT_EQ(executor.Execute({"ping"}).Encode(), "+PONG\r\n");
    EXPECT_EQ(executor.Execute({"SET", "key", "value"}).Encode(), "+OK\r\n");
    EXPECT_EQ(executor.Execute({"GET", "key"}).Encode(), "$5\r\nvalue\r\n");
    EXPECT_EQ(executor.Execute({"EXISTS", "key", "missing"}).Encode(), ":1\r\n");
    EXPECT_EQ(executor.Execute({"DEL", "key"}).Encode(), ":1\r\n");
    EXPECT_EQ(executor.Execute({"GET", "key"}).Encode(), "$-1\r\n");
}

TEST_CASE("SET options and integer commands are validated") {
    FakeDatabase database;
    cachefly::command::CommandExecutor executor(&database);
    EXPECT_EQ(executor.Execute({"SET", "counter", "10", "NX", "EX", "5"}).Encode(), "+OK\r\n");
    EXPECT_EQ(executor.Execute({"SET", "counter", "11", "NX"}).Encode(), "$-1\r\n");
    EXPECT_EQ(executor.Execute({"INCR", "counter"}).Encode(), ":11\r\n");
    EXPECT_EQ(executor.Execute({"DECR", "counter"}).Encode(), ":10\r\n");
    EXPECT_TRUE(StartsWith(executor.Execute({"SET", "x", "y", "EX", "bad"}).Encode(), "-ERR"));
}

TEST_CASE("MGET MSET and command errors") {
    FakeDatabase database;
    cachefly::command::CommandExecutor executor(&database);
    EXPECT_EQ(executor.Execute({"MSET", "a", "1", "b", "2"}).Encode(), "+OK\r\n");
    EXPECT_EQ(executor.Execute({"MGET", "a", "none", "b"}).Encode(),
              "*3\r\n$1\r\n1\r\n$-1\r\n$1\r\n2\r\n");
    EXPECT_TRUE(StartsWith(executor.Execute({"GET"}).Encode(), "-ERR"));
    EXPECT_TRUE(StartsWith(executor.Execute({"NOPE"}).Encode(), "-ERR"));
}

TEST_CASE("failed MSET neither mutates nor emits an AOF callback") {
    FakeDatabase database;
    database.fail_mset = true;
    cachefly::command::CommandExecutor executor(&database);
    std::size_t mutations = 0;
    executor.SetMutationCallback([&mutations](const auto&) { ++mutations; });
    EXPECT_TRUE(StartsWith(
        executor.Execute({"MSET", "a", "1", "b", "2"}).Encode(), "-OOM"));
    EXPECT_TRUE(database.values.empty());
    EXPECT_EQ(mutations, 0U);
}

TEST_CASE("mutation guard rejects writes before database changes") {
    FakeDatabase database;
    cachefly::command::CommandExecutor executor(&database);
    std::size_t mutations = 0;
    executor.SetMutationGuard([] { throw std::runtime_error("persistence failed"); });
    executor.SetMutationCallback([&mutations](const auto&) { ++mutations; });
    EXPECT_TRUE(StartsWith(
        executor.Execute({"SET", "key", "value"}).Encode(), "-MISCONF"));
    EXPECT_TRUE(database.values.empty());
    EXPECT_EQ(mutations, 0U);
}

TEST_CASE("async dispatcher preserves command order within a session") {
    FakeDatabase database;
    cachefly::command::CommandExecutor executor(&database);
    cachefly::command::AsyncDispatcher dispatcher(&executor, 2);
    dispatcher.OpenSession(7);
    std::mutex replies_mutex;
    std::vector<std::string> replies;
    const auto collect = [&replies_mutex, &replies](std::string response) {
        std::lock_guard lock(replies_mutex);
        replies.push_back(std::move(response));
    };
    dispatcher.Submit(7, {"SET", "ordered", "value"}, collect);
    dispatcher.Submit(7, {"GET", "ordered"}, collect);
    dispatcher.Stop();
    EXPECT_EQ(replies.size(), 2U);
    EXPECT_EQ(replies[0], "+OK\r\n");
    EXPECT_EQ(replies[1], "$5\r\nvalue\r\n");
}
