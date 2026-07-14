#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "cachefly/base/noncopyable.h"
#include "cachefly/net/buffer.h"

namespace cachefly::net {

class Channel;
class EventLoop;

class TcpConnection final : public cachefly::NonCopyable,
                            public std::enable_shared_from_this<TcpConnection> {
public:
    using Ptr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(const Ptr&)>;
    using MessageCallback = std::function<void(const Ptr&, Buffer*)>;
    using CloseCallback = std::function<void(const Ptr&)>;

    TcpConnection(EventLoop* loop, int fd, std::string peer);
    ~TcpConnection();

    [[nodiscard]] int Fd() const noexcept;
    [[nodiscard]] const std::string& Peer() const noexcept;
    [[nodiscard]] bool Connected() const noexcept;
    void Send(std::string message);
    void Shutdown();

    void SetConnectionCallback(ConnectionCallback callback);
    void SetMessageCallback(MessageCallback callback);
    void SetCloseCallback(CloseCallback callback);
    void ConnectEstablished();
    void ConnectDestroyed();

private:
    enum class State { kConnecting, kConnected, kDisconnecting, kDisconnected };

    void HandleRead();
    void HandleWrite();
    void HandleClose();
    void HandleError();
    void SendInLoop(std::string message);
    void ShutdownInLoop();

    EventLoop* loop_;
    const int fd_;
    const std::string peer_;
    std::unique_ptr<Channel> channel_;
    std::atomic<State> state_{State::kConnecting};
    Buffer input_buffer_;
    Buffer output_buffer_;
    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    CloseCallback close_callback_;
};

}  // namespace cachefly::net
