#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "cachefly/base/noncopyable.h"

namespace cachefly::command {

class CommandExecutor;
class ThreadPool;

class AsyncDispatcher final : public cachefly::NonCopyable {
public:
    using SessionId = std::uint64_t;
    using ReplyCallback = std::function<void(std::string)>;

    AsyncDispatcher(CommandExecutor* executor, std::size_t thread_count);
    ~AsyncDispatcher();

    void OpenSession(SessionId id);
    void CloseSession(SessionId id);
    void Submit(SessionId id,
                std::vector<std::string> arguments,
                ReplyCallback reply);
    void Stop();

private:
    struct PendingCommand {
        std::vector<std::string> arguments;
        ReplyCallback reply;
    };

    struct Session {
        std::mutex mutex;
        std::deque<PendingCommand> commands;
        bool running{false};
        bool closed{false};
    };

    void RunSession(const std::shared_ptr<Session>& session);

    CommandExecutor* executor_;
    std::unique_ptr<ThreadPool> pool_;
    std::mutex sessions_mutex_;
    std::unordered_map<SessionId, std::shared_ptr<Session>> sessions_;
    bool stopped_{false};
};

}  // namespace cachefly::command
