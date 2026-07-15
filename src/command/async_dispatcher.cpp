#include "cachefly/command/async_dispatcher.h"

#include <condition_variable>
#include <stdexcept>
#include <thread>
#include <utility>

#include "cachefly/base/logger.h"
#include "cachefly/command/command_executor.h"
#include "cachefly/resp/resp_value.h"

namespace cachefly::command {

class ThreadPool final : public cachefly::NonCopyable {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(std::size_t thread_count) {
        if (thread_count == 0) throw std::invalid_argument("thread pool must not be empty");
        workers_.reserve(thread_count);
        for (std::size_t index = 0; index < thread_count; ++index) {
            workers_.emplace_back([this] { Run(); });
        }
    }

    ~ThreadPool() { Stop(); }

    void Post(Task task) {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) throw std::runtime_error("thread pool is stopping");
            tasks_.push_back(std::move(task));
        }
        condition_.notify_one();
    }

    void Stop() {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) return;
            stopping_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
        workers_.clear();
    }

private:
    void Run() {
        while (true) {
            Task task;
            {
                std::unique_lock lock(mutex_);
                condition_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            try {
                task();
            } catch (const std::exception& error) {
                LOG_ERROR("command worker failed: " + std::string(error.what()));
            }
        }
    }

    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Task> tasks_;
    bool stopping_{false};
    std::vector<std::thread> workers_;
};

AsyncDispatcher::AsyncDispatcher(CommandExecutor* executor, std::size_t thread_count)
    : executor_(executor), pool_(std::make_unique<ThreadPool>(thread_count)) {
    if (executor_ == nullptr) throw std::invalid_argument("executor must not be null");
}

AsyncDispatcher::~AsyncDispatcher() { Stop(); }

void AsyncDispatcher::OpenSession(SessionId id) {
    std::lock_guard lock(sessions_mutex_);
    if (stopped_) throw std::runtime_error("dispatcher is stopped");
    sessions_.insert_or_assign(id, std::make_shared<Session>());
}

void AsyncDispatcher::CloseSession(SessionId id) {
    std::shared_ptr<Session> session;
    {
        std::lock_guard lock(sessions_mutex_);
        const auto found = sessions_.find(id);
        if (found == sessions_.end()) return;
        session = found->second;
        sessions_.erase(found);
    }
    std::lock_guard lock(session->mutex);
    session->closed = true;
    session->commands.clear();
}

void AsyncDispatcher::Submit(SessionId id,
                             std::vector<std::string> arguments,
                             ReplyCallback reply) {
    std::shared_ptr<Session> session;
    {
        std::lock_guard lock(sessions_mutex_);
        if (stopped_) throw std::runtime_error("dispatcher is stopped");
        const auto found = sessions_.find(id);
        if (found == sessions_.end()) return;
        session = found->second;
    }

    bool schedule = false;
    {
        std::lock_guard lock(session->mutex);
        if (session->closed) return;
        session->commands.push_back({std::move(arguments), std::move(reply)});
        if (!session->running) {
            session->running = true;
            schedule = true;
        }
    }
    if (schedule) {
        pool_->Post([this, session] { RunSession(session); });
    }
}

void AsyncDispatcher::Stop() {
    {
        std::lock_guard lock(sessions_mutex_);
        if (stopped_) return;
        stopped_ = true;
    }
    pool_->Stop();
    std::lock_guard lock(sessions_mutex_);
    sessions_.clear();
}

void AsyncDispatcher::RunSession(const std::shared_ptr<Session>& session) {
    while (true) {
        PendingCommand command;
        {
            std::lock_guard lock(session->mutex);
            if (session->closed || session->commands.empty()) {
                session->running = false;
                return;
            }
            command = std::move(session->commands.front());
            session->commands.pop_front();
        }

        std::string response;
        try {
            response = executor_->Execute(command.arguments).Encode();
        } catch (const std::exception& error) {
            LOG_ERROR("command execution failed: " + std::string(error.what()));
            response = resp::Value::Error("ERR internal server error").Encode();
        }

        {
            std::lock_guard lock(session->mutex);
            if (session->closed) return;
        }
        command.reply(std::move(response));
    }
}

}  // namespace cachefly::command
