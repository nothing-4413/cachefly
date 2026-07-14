#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "cachefly/base/noncopyable.h"

namespace cachefly::net {

class Channel;

class EventLoop final : public cachefly::NonCopyable {
public:
    using Task = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void Loop();
    void Quit();
    void RunInLoop(Task task);
    void QueueInLoop(Task task);

    [[nodiscard]] bool IsInLoopThread() const noexcept;
    void AssertInLoopThread() const;
    void UpdateChannel(Channel* channel);
    void RemoveChannel(Channel* channel);

private:
    void Wakeup();
    void HandleWakeup();
    void RunPendingTasks();

    const std::thread::id owner_thread_;
    const int epoll_fd_;
    const int wakeup_fd_;
    std::unique_ptr<Channel> wakeup_channel_;
    std::atomic<bool> looping_{false};
    std::atomic<bool> quit_{false};
    std::mutex pending_mutex_;
    std::vector<Task> pending_tasks_;
};

}  // namespace cachefly::net
