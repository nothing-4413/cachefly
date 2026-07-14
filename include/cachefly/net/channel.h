#pragma once

#include <cstdint>
#include <functional>

#include "cachefly/base/noncopyable.h"

namespace cachefly::net {

class EventLoop;

class Channel final : public cachefly::NonCopyable {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    void HandleEvent();
    void SetReadCallback(EventCallback callback);
    void SetWriteCallback(EventCallback callback);
    void SetCloseCallback(EventCallback callback);
    void SetErrorCallback(EventCallback callback);

    [[nodiscard]] int Fd() const noexcept;
    [[nodiscard]] std::uint32_t Events() const noexcept;
    void SetReturnedEvents(std::uint32_t events) noexcept;
    [[nodiscard]] bool IsWriting() const noexcept;
    [[nodiscard]] bool HasNoEvents() const noexcept;

    void EnableReading();
    void EnableWriting();
    void DisableWriting();
    void DisableAll();
    void Remove();

private:
    void Update();

    EventLoop* loop_;
    const int fd_;
    std::uint32_t events_{0};
    std::uint32_t returned_events_{0};
    bool added_to_loop_{false};
    EventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback close_callback_;
    EventCallback error_callback_;

    friend class EventLoop;
};

}  // namespace cachefly::net
