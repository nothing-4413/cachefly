#include "cachefly/net/event_loop.h"

#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "cachefly/base/logger.h"
#include "cachefly/net/channel.h"

namespace cachefly::net {
namespace {

int CreateEpoll() {
    const int fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "epoll_create1");
    return fd;
}

int CreateEventFd() {
    const int fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "eventfd");
    return fd;
}

}  // namespace

EventLoop::EventLoop()
    : owner_thread_(std::this_thread::get_id()),
      epoll_fd_(CreateEpoll()),
      wakeup_fd_(CreateEventFd()),
      wakeup_channel_(std::make_unique<Channel>(this, wakeup_fd_)) {
    wakeup_channel_->SetReadCallback([this] { HandleWakeup(); });
    wakeup_channel_->EnableReading();
}

EventLoop::~EventLoop() {
    wakeup_channel_->DisableAll();
    wakeup_channel_->Remove();
    ::close(wakeup_fd_);
    ::close(epoll_fd_);
}

void EventLoop::Loop() {
    AssertInLoopThread();
    if (looping_.exchange(true)) throw std::logic_error("EventLoop already running");
    std::vector<epoll_event> events(64);
    while (!quit_.load(std::memory_order_relaxed)) {
        const int ready = ::epoll_wait(epoll_fd_, events.data(),
                                       static_cast<int>(events.size()), 10000);
        if (ready < 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "epoll_wait");
        }
        for (int index = 0; index < ready; ++index) {
            const std::size_t slot = static_cast<std::size_t>(index);
            auto* channel = static_cast<Channel*>(events[slot].data.ptr);
            channel->SetReturnedEvents(events[slot].events);
            channel->HandleEvent();
        }
        if (ready == static_cast<int>(events.size())) events.resize(events.size() * 2);
        RunPendingTasks();
    }
    RunPendingTasks();
    looping_.store(false);
}

void EventLoop::Quit() { quit_.store(true, std::memory_order_relaxed); Wakeup(); }

void EventLoop::RunInLoop(Task task) {
    if (IsInLoopThread()) task();
    else QueueInLoop(std::move(task));
}

void EventLoop::QueueInLoop(Task task) {
    {
        std::lock_guard lock(pending_mutex_);
        pending_tasks_.push_back(std::move(task));
    }
    Wakeup();
}

bool EventLoop::IsInLoopThread() const noexcept {
    return owner_thread_ == std::this_thread::get_id();
}

void EventLoop::AssertInLoopThread() const {
    if (!IsInLoopThread()) throw std::logic_error("operation must run in EventLoop thread");
}

void EventLoop::UpdateChannel(Channel* channel) {
    AssertInLoopThread();
    epoll_event event{};
    event.events = channel->Events();
    event.data.ptr = channel;
    int operation = EPOLL_CTL_MOD;
    if (!channel->added_to_loop_) {
        if (channel->HasNoEvents()) return;
        operation = EPOLL_CTL_ADD;
        channel->added_to_loop_ = true;
    } else if (channel->HasNoEvents()) {
        operation = EPOLL_CTL_DEL;
        channel->added_to_loop_ = false;
    }
    if (::epoll_ctl(epoll_fd_, operation, channel->Fd(), &event) < 0 &&
        !(operation == EPOLL_CTL_DEL && errno == ENOENT)) {
        throw std::system_error(errno, std::generic_category(), "epoll_ctl");
    }
}

void EventLoop::RemoveChannel(Channel* channel) {
    AssertInLoopThread();
    if (!channel->added_to_loop_) return;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, channel->Fd(), nullptr) < 0 && errno != ENOENT) {
        LOG_ERROR("epoll_ctl DEL failed");
    }
    channel->added_to_loop_ = false;
}

void EventLoop::Wakeup() {
    const std::uint64_t one = 1;
    const ssize_t written = ::write(wakeup_fd_, &one, sizeof(one));
    if (written < 0 && errno != EAGAIN) LOG_ERROR("eventfd write failed");
}

void EventLoop::HandleWakeup() {
    std::uint64_t value = 0;
    const ssize_t count = ::read(wakeup_fd_, &value, sizeof(value));
    if (count < 0 && errno != EAGAIN) LOG_ERROR("eventfd read failed");
}

void EventLoop::RunPendingTasks() {
    std::vector<Task> tasks;
    {
        std::lock_guard lock(pending_mutex_);
        tasks.swap(pending_tasks_);
    }
    for (auto& task : tasks) task();
}

}  // namespace cachefly::net
