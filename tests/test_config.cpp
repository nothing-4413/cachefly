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
    EXPECT_EQ(config.port, 6379);
    EXPECT_EQ(config.shard_threads, 4U);
}

TEST_CASE("memory size parsing") {
    EXPECT_EQ(cachefly::ConfigLoader::ParseMemorySize("1kb"), 1024U);
    EXPECT_EQ(cachefly::ConfigLoader::ParseMemorySize("1.5 MiB"), 1572864U);
    EXPECT_THROW(cachefly::ConfigLoader::ParseMemorySize("-1mb"), std::invalid_argument);
    EXPECT_THROW(cachefly::ConfigLoader::ParseMemorySize("12parsecs"), std::invalid_argument);
}

TEST_CASE("file config and command line precedence") {
    TemporaryFile file("port=6380\nshard_threads=2\n");
    const auto config = LoadArgs({"cachefly", "--port=6381",
                                  "--config=" + file.Path(), "--shard_threads=8"});
    EXPECT_EQ(config.port, 6381);
    EXPECT_EQ(config.shard_threads, 8U);
}

TEST_CASE("invalid config rejected") {
    EXPECT_THROW(LoadArgs({"cachefly", "--port=0"}), std::invalid_argument);
    EXPECT_THROW(LoadArgs({"cachefly", "--shard_threads=-1"}), std::invalid_argument);
    EXPECT_THROW(LoadArgs({"cachefly", "--unknown=value"}), std::invalid_argument);
}

