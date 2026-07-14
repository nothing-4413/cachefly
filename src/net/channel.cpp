#include "cachefly/net/channel.h"

#include <sys/epoll.h>

#include <utility>

#include "cachefly/net/event_loop.h"

namespace cachefly::net {
namespace {
constexpr std::uint32_t kReadEvents = static_cast<std::uint32_t>(EPOLLIN | EPOLLPRI | EPOLLRDHUP);
constexpr std::uint32_t kWriteEvent = static_cast<std::uint32_t>(EPOLLOUT);
}  // namespace

Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd) {}

void Channel::HandleEvent() {
    const auto hangup = static_cast<std::uint32_t>(EPOLLHUP);
    const auto input = static_cast<std::uint32_t>(EPOLLIN);
    const auto error = static_cast<std::uint32_t>(EPOLLERR);
    if ((returned_events_ & hangup) != 0U && (returned_events_ & input) == 0U) {
        if (close_callback_) close_callback_();
        return;
    }
    if ((returned_events_ & error) != 0U && error_callback_) error_callback_();
    if ((returned_events_ & kReadEvents) != 0U && read_callback_) read_callback_();
    if ((returned_events_ & kWriteEvent) != 0U && write_callback_) write_callback_();
}

void Channel::SetReadCallback(EventCallback callback) { read_callback_ = std::move(callback); }
void Channel::SetWriteCallback(EventCallback callback) { write_callback_ = std::move(callback); }
void Channel::SetCloseCallback(EventCallback callback) { close_callback_ = std::move(callback); }
void Channel::SetErrorCallback(EventCallback callback) { error_callback_ = std::move(callback); }
int Channel::Fd() const noexcept { return fd_; }
std::uint32_t Channel::Events() const noexcept { return events_; }
void Channel::SetReturnedEvents(std::uint32_t events) noexcept { returned_events_ = events; }
bool Channel::IsWriting() const noexcept { return (events_ & kWriteEvent) != 0U; }
bool Channel::HasNoEvents() const noexcept { return events_ == 0U; }
void Channel::EnableReading() { events_ |= kReadEvents; Update(); }
void Channel::EnableWriting() { events_ |= kWriteEvent; Update(); }
void Channel::DisableWriting() { events_ &= ~kWriteEvent; Update(); }
void Channel::DisableAll() { events_ = 0; Update(); }
void Channel::Remove() { loop_->RemoveChannel(this); }
void Channel::Update() { loop_->UpdateChannel(this); }

}  // namespace cachefly::net
