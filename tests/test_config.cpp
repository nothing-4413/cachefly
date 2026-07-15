#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cachefly/config/config.h"
#include "test_harness.h"

namespace {

cachefly::ServerConfig LoadArgs(std::vector<std::string> arguments) {
    std::vector<char*> argv;
    for (std::string& argument : arguments) argv.push_back(argument.data());
    return cachefly::ConfigLoader::LoadFromArgs(static_cast<int>(argv.size()), argv.data());
}

class TemporaryFile {
public:
    explicit TemporaryFile(std::string contents) : path_("cachefly_test.conf") {
        std::ofstream output(path_);
        output << contents;
        if (!output) throw std::runtime_error("cannot write test config");
    }
    ~TemporaryFile() { std::remove(path_.c_str()); }
    [[nodiscard]] const std::string& Path() const { return path_; }

private:
    std::string path_;
};

}  // namespace

TEST_CASE("default config") {
    const cachefly::ServerConfig config;
    EXPECT_EQ(config.bind_address, "127.0.0.1");
    EXPECT_EQ(config.port, 6379);
    EXPECT_EQ(config.shard_threads, 4U);
    EXPECT_EQ(config.max_clients, 10000U);
    EXPECT_EQ(config.max_request_bytes, 16U * 1024U * 1024U);
    EXPECT_EQ(config.max_output_bytes, 64U * 1024U * 1024U);
}

TEST_CASE("memory size parsing") {
    EXPECT_EQ(cachefly::ConfigLoader::ParseMemorySize("1kb"), 1024U);
    EXPECT_EQ(cachefly::ConfigLoader::ParseMemorySize("1.5 MiB"), 1572864U);
    EXPECT_THROW(cachefly::ConfigLoader::ParseMemorySize("-1mb"), std::invalid_argument);
    EXPECT_THROW(cachefly::ConfigLoader::ParseMemorySize("12parsecs"), std::invalid_argument);
}

TEST_CASE("file config and command line precedence") {
    TemporaryFile file("port=6380\nshard_threads=2\nmax_clients=100\n"
                       "max_request_bytes=2mb\nmax_output_bytes=3mb\n");
    const auto config = LoadArgs({"cachefly", "--port=6381",
                                  "--config=" + file.Path(), "--shard_threads=8",
                                  "--max_clients=200", "--max_request_bytes=4mb"});
    EXPECT_EQ(config.port, 6381);
    EXPECT_EQ(config.shard_threads, 8U);
    EXPECT_EQ(config.max_clients, 200U);
    EXPECT_EQ(config.max_request_bytes, 4U * 1024U * 1024U);
    EXPECT_EQ(config.max_output_bytes, 3U * 1024U * 1024U);
}

TEST_CASE("invalid config rejected") {
    EXPECT_THROW(LoadArgs({"cachefly", "--port=0"}), std::invalid_argument);
    EXPECT_THROW(LoadArgs({"cachefly", "--shard_threads=-1"}), std::invalid_argument);
    EXPECT_THROW(LoadArgs({"cachefly", "--max_clients=0"}), std::invalid_argument);
    EXPECT_THROW(LoadArgs({"cachefly", "--max_request_bytes=0"}), std::invalid_argument);
    EXPECT_THROW(LoadArgs({"cachefly", "--max_output_bytes=0"}), std::invalid_argument);
    EXPECT_THROW(LoadArgs({"cachefly", "--unknown=value"}), std::invalid_argument);
    EXPECT_THROW(LoadArgs({"cachefly", "--io_threads=2"}), std::invalid_argument);
}

TEST_CASE("effective config JSON includes every runtime option") {
    cachefly::ServerConfig config;
    config.log_file = "logs/quoted\"name.log";
    const std::string json = cachefly::ConfigLoader::ToJson(config);
    const std::vector<std::string> keys{
        "bind", "port", "shard_threads", "max_clients", "max_request_bytes",
        "max_output_bytes", "maxmemory_bytes", "eviction_policy", "log_level",
        "log_file", "appendonly", "appendfilename", "appendfsync", "snapshot",
        "snapshotfilename", "admin_port"};
    for (const auto& key : keys) {
        EXPECT_TRUE(json.find("\"" + key + "\":") != std::string::npos);
    }
    EXPECT_TRUE(json.find("quoted\\\"name.log") != std::string::npos);
}
