#include <csignal>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>

#include "cachefly/base/logger.h"
#include "cachefly/command/command_executor.h"
#include "cachefly/config/config.h"
#include "cachefly/net/buffer.h"
#include "cachefly/net/event_loop.h"
#include "cachefly/net/tcp_connection.h"
#include "cachefly/net/tcp_server.h"
#include "cachefly/persist/aof.h"
#include "cachefly/persist/snapshot.h"
#include "cachefly/resp/resp_parser.h"
#include "cachefly/resp/resp_value.h"
#include "cachefly/shard/sharded_database.h"
#include "cachefly/storage/kv_store.h"

namespace {

cachefly::net::EventLoop* g_loop = nullptr;

void HandleSignal(int) {
    if (g_loop != nullptr) g_loop->Quit();
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
            throw std::runtime_error("failed to open log file: " + config.log_file);
        }

        cachefly::shard::ShardedDatabase database(
            config.shard_threads, config.maxmemory_bytes,
            cachefly::storage::ParseEvictionPolicy(config.eviction_policy));
        cachefly::command::CommandExecutor executor(&database);

        const auto replay = [&executor](const std::vector<std::string>& command) {
            const auto result = executor.Execute(command);
            if (result.type == cachefly::resp::Type::kError) {
                throw std::runtime_error("recovery command failed: " + result.string);
            }
        };
        if (config.appendonly) {
            const std::size_t recovered = cachefly::persist::AofWriter::Replay(
                config.appendfilename, replay);
            LOG_INFO("replayed " + std::to_string(recovered) + " AOF commands");
        } else if (config.snapshot) {
            const std::size_t recovered = cachefly::persist::Snapshot::Load(
                config.snapshotfilename, replay);
            LOG_INFO("loaded " + std::to_string(recovered) + " snapshot entries");
        }

        std::unique_ptr<cachefly::persist::AofWriter> aof;
        if (config.appendonly) {
            aof = std::make_unique<cachefly::persist::AofWriter>(
                config.appendfilename,
                cachefly::persist::ParseFsyncPolicy(config.appendfsync));
            executor.SetMutationCallback([&aof](const std::vector<std::string>& command) {
                aof->Append(command);
            });
        }
        cachefly::resp::Parser parser;
        cachefly::net::EventLoop loop;
        g_loop = &loop;
        std::signal(SIGINT, HandleSignal);
        std::signal(SIGTERM, HandleSignal);

        cachefly::net::TcpServer server(&loop, config.bind_address, config.port);
        server.SetConnectionCallback([](const cachefly::net::TcpConnection::Ptr& connection) {
            LOG_INFO(std::string(connection->Connected() ? "connected: " : "disconnected: ") +
                     connection->Peer());
        });
        server.SetMessageCallback(
            [&parser, &executor](const cachefly::net::TcpConnection::Ptr& connection,
                                cachefly::net::Buffer* input) {
                while (input->ReadableBytes() > 0) {
                    std::size_t consumed = 0;
                    std::vector<std::string> arguments;
                    std::string error;
                    const auto result = parser.ParseCommand(input->View(), &consumed,
                                                             &arguments, &error);
                    if (result == cachefly::resp::ParseResult::kIncomplete) return;
                    if (result == cachefly::resp::ParseResult::kError) {
                        input->RetrieveAll();
                        connection->Send(cachefly::resp::Value::Error("ERR Protocol error: " + error).Encode());
                        connection->Shutdown();
                        return;
                    }
                    input->Retrieve(consumed);
                    connection->Send(executor.Execute(arguments).Encode());
                }
            });
        server.Start();
        LOG_INFO("cachefly is ready");
        loop.Loop();
        g_loop = nullptr;
        if (config.snapshot) {
            cachefly::persist::Snapshot::Save(config.snapshotfilename,
                                               database.Snapshot());
            LOG_INFO("snapshot saved");
        }
        LOG_INFO("cachefly stopped");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal error: " << error.what() << '\n';
        return 1;
    }
}
