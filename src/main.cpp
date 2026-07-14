#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "cachefly/base/logger.h"
#include "cachefly/config/config.h"

namespace {

std::string Summary(const cachefly::ServerConfig& config) {
    std::ostringstream output;
    output << "bind=" << config.bind_address << ", port=" << config.port
           << ", io_threads=" << config.io_threads
           << ", shard_threads=" << config.shard_threads
           << ", maxmemory=" << config.maxmemory_bytes
           << ", eviction_policy=" << config.eviction_policy;
    return output.str();
}

void Usage(const char* executable) {
    std::cout << "Usage: " << executable << " [--config=PATH] [--key=value ...]\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        Usage(argv[0]);
        return 0;
    }
    try {
        const auto config = cachefly::ConfigLoader::LoadFromArgs(argc, argv);
        auto& logger = cachefly::Logger::Instance();
        logger.SetLevel(cachefly::ParseLogLevel(config.log_level));
        if (!logger.SetOutputFile(config.log_file)) {
            std::cerr << "failed to open log file: " << config.log_file << '\n';
            return 1;
        }
        LOG_INFO("cachefly starting");
        LOG_INFO(Summary(config));
        LOG_INFO("module 1 initialized successfully");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal error: " << error.what() << '\n';
        Usage(argv[0]);
        return 1;
    }
}

