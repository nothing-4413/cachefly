#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cachefly {

struct ServerConfig {
    std::string bind_address{"0.0.0.0"};
    std::uint16_t port{6379};
    std::size_t io_threads{1};
    std::size_t shard_threads{4};
    std::size_t maxmemory_bytes{512ULL * 1024ULL * 1024ULL};
    std::string eviction_policy{"lru"};
    std::string log_level{"info"};
    std::string log_file;
    bool appendonly{false};
    std::string appendfilename{"cachefly.aof"};
    std::string appendfsync{"everysec"};
    bool snapshot{false};
    std::string snapshotfilename{"cachefly.snapshot"};
    std::uint16_t admin_port{8080};
};

class ConfigLoader {
public:
    [[nodiscard]] static ServerConfig LoadFromFile(const std::string& path);
    [[nodiscard]] static ServerConfig LoadFromArgs(int argc, char* argv[]);
    [[nodiscard]] static std::size_t ParseMemorySize(const std::string& text);
    static void Validate(const ServerConfig& config);

private:
    static void Apply(ServerConfig& config,
                      const std::string& key,
                      const std::string& value);
};

}  // namespace cachefly
