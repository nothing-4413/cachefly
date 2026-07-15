#include <cstdio>
#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <utility>

#include "cachefly/persist/aof.h"
#include "cachefly/persist/snapshot.h"
#include "test_harness.h"

namespace {
class TemporaryPath {
public:
    explicit TemporaryPath(std::string path) : path_(std::move(path)) {
        std::remove(path_.c_str());
        std::remove((path_ + ".tmp").c_str());
    }
    ~TemporaryPath() {
        std::remove(path_.c_str());
        std::remove((path_ + ".tmp").c_str());
    }
    const std::string& Get() const { return path_; }
private:
    std::string path_;
};
}  // namespace

TEST_CASE("AOF appends and replays complete RESP commands") {
    TemporaryPath path("cachefly-test.aof");
    {
        cachefly::persist::AofWriter writer(
            path.Get(), cachefly::persist::FsyncPolicy::kAlways);
        writer.Append({"SET", "key", "value"});
        writer.Append({"INCR", "counter"});
    }
    std::vector<std::vector<std::string>> commands;
    const std::size_t count = cachefly::persist::AofWriter::Replay(
        path.Get(), [&commands](const auto& command) { commands.push_back(command); });
    EXPECT_EQ(count, 2U);
    EXPECT_EQ(commands[0][2], "value");
}

TEST_CASE("snapshot round trips values and remaining TTL") {
    TemporaryPath path("cachefly-test.snapshot");
    cachefly::persist::Snapshot::Save(path.Get(), {
        {"plain", "one", std::nullopt},
        {"expiring", "two", std::chrono::milliseconds(5000)}});
    std::vector<std::vector<std::string>> commands;
    EXPECT_EQ(cachefly::persist::Snapshot::Load(
                  path.Get(), [&commands](const auto& command) { commands.push_back(command); }),
              2U);
    EXPECT_EQ(commands[0][0], "SET");
    EXPECT_EQ(commands[1].size(), 5U);
}
