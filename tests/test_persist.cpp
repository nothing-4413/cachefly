#include <cstdio>
#include <chrono>
#include <optional>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
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

TEST_CASE("snapshot atomically replaces an existing file and removes its temporary") {
    TemporaryPath path("cachefly-replaced.snapshot");
    cachefly::persist::Snapshot::Save(path.Get(), {{"old", "one", std::nullopt}});
    cachefly::persist::Snapshot::Save(path.Get(), {{"new", "two", std::nullopt}});
    std::vector<std::vector<std::string>> commands;
    EXPECT_EQ(cachefly::persist::Snapshot::Load(
                  path.Get(), [&commands](const auto& command) { commands.push_back(command); }),
              1U);
    EXPECT_EQ(commands[0][1], "new");
    EXPECT_TRUE(!std::filesystem::exists(path.Get() + ".tmp"));
}

TEST_CASE("asynchronous AOF failures become permanent and observable") {
    cachefly::persist::AofWriter writer(
        "/dev/full", cachefly::persist::FsyncPolicy::kNever);
    writer.Append({"SET", "key", "value"});
    bool failed = false;
    for (int attempt = 0; attempt < 100 && !failed; ++attempt) {
        try {
            writer.CheckError();
        } catch (const std::system_error&) {
            failed = true;
        }
        if (!failed) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_TRUE(failed);
    EXPECT_THROW(writer.Append({"SET", "another", "value"}), std::system_error);
}

TEST_CASE("AOF recovery truncates an incomplete tail before new appends") {
    TemporaryPath path("cachefly-truncated.aof");
    const std::string valid = cachefly::resp::EncodeCommand({"SET", "key", "value"});
    {
        std::ofstream output(path.Get(), std::ios::binary);
        output << valid << "*2\r\n$3\r\nSET\r\n$4\r\npart";
    }
    std::vector<std::vector<std::string>> commands;
    EXPECT_EQ(cachefly::persist::AofWriter::Replay(
                  path.Get(), [&commands](const auto& command) { commands.push_back(command); }, true),
              1U);
    EXPECT_EQ(std::filesystem::file_size(path.Get()), valid.size());
    {
        cachefly::persist::AofWriter writer(
            path.Get(), cachefly::persist::FsyncPolicy::kAlways);
        writer.Append({"INCR", "counter"});
    }
    EXPECT_EQ(cachefly::persist::AofWriter::Replay(
                  path.Get(), [](const auto&) {}),
              2U);
}
