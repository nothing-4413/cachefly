#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "cachefly/base/noncopyable.h"
#include "cachefly/net/socket.h"
#include "cachefly/net/tcp_connection.h"

namespace cachefly::net {

class Channel;
class EventLoop;

class TcpServer final : public cachefly::NonCopyable {
public:
    TcpServer(EventLoop* loop, std::string address, std::uint16_t port);
    ~TcpServer();

    void SetConnectionCallback(TcpConnection::ConnectionCallback callback);
    void SetMessageCallback(TcpConnection::MessageCallback callback);
    void SetTrafficCallback(TcpConnection::TrafficCallback callback);
    void Start();
    [[nodiscard]] std::size_t ConnectionCount() const noexcept;

private:
    void HandleAccept();
    void RemoveConnection(const TcpConnection::Ptr& connection);

    EventLoop* loop_;
    Socket listen_socket_;
    std::unique_ptr<Channel> listen_channel_;
    std::string address_;
    std::uint16_t port_;
    bool started_{false};
    std::unordered_map<int, TcpConnection::Ptr> connections_;
    TcpConnection::ConnectionCallback connection_callback_;
    TcpConnection::MessageCallback message_callback_;
    TcpConnection::TrafficCallback traffic_callback_;
};

}  // namespace cachefly::net
