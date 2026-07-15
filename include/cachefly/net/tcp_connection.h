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

struct TcpConnectionOptions {
    std::size_t max_request_bytes{16ULL * 1024ULL * 1024ULL};
    std::size_t max_output_bytes{64ULL * 1024ULL * 1024ULL};
};

class TcpConnection final : public cachefly::NonCopyable,
                            public std::enable_shared_from_this<TcpConnection> {
public:
    using Ptr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(const Ptr&)>;
    using MessageCallback = std::function<void(const Ptr&, Buffer*)>;
    using CloseCallback = std::function<void(const Ptr&)>;
    using TrafficCallback = std::function<void(std::size_t, std::size_t)>;

    TcpConnection(EventLoop* loop,
                  int fd,
                  std::string peer,
                  TcpConnectionOptions options = {});
    ~TcpConnection();

    [[nodiscard]] int Fd() const noexcept;
    [[nodiscard]] const std::string& Peer() const noexcept;
    [[nodiscard]] bool Connected() const noexcept;
    void Send(std::string message);
    void Shutdown();

    void SetConnectionCallback(ConnectionCallback callback);
    void SetMessageCallback(MessageCallback callback);
    void SetCloseCallback(CloseCallback callback);
    void SetTrafficCallback(TrafficCallback callback);
    void ConnectEstablished();
    void ConnectDestroyed();

private:
    enum class State { kConnecting, kConnected, kDisconnecting, kDisconnected };

    void HandleRead();
    void HandleWrite();
    void HandleClose();
    void HandleError();
    void ForceClose();
    bool ReserveOutput(std::size_t bytes);
    void ReleaseOutput(std::size_t bytes);
    void SendInLoop(std::string message);
    void ShutdownInLoop();

    EventLoop* loop_;
    const int fd_;
    const std::string peer_;
    std::unique_ptr<Channel> channel_;
    std::atomic<State> state_{State::kConnecting};
    const std::size_t max_request_bytes_;
    const std::size_t max_output_bytes_;
    std::atomic<std::size_t> pending_output_bytes_{0};
    Buffer input_buffer_;
    Buffer output_buffer_;
    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    CloseCallback close_callback_;
    TrafficCallback traffic_callback_;
};

}  // namespace cachefly::net
