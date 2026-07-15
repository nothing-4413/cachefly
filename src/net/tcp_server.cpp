#include "cachefly/net/tcp_server.h"

#include <utility>

#include <unistd.h>

#include "cachefly/base/logger.h"
#include "cachefly/net/channel.h"
#include "cachefly/net/event_loop.h"

namespace cachefly::net {

TcpServer::TcpServer(EventLoop* loop,
                     std::string address,
                     std::uint16_t port,
                     TcpServerOptions options)
    : loop_(loop), listen_socket_(Socket::CreateNonBlocking()),
      listen_channel_(std::make_unique<Channel>(loop, listen_socket_.Fd())),
      address_(std::move(address)), port_(port), options_(options) {
    listen_socket_.SetReuseAddress(true);
    listen_socket_.Bind(address_, port_);
    listen_channel_->SetReadCallback([this] { HandleAccept(); });
}

TcpServer::~TcpServer() {
    loop_->AssertInLoopThread();
    listen_channel_->DisableAll();
    listen_channel_->Remove();
    while (!connections_.empty()) connections_.begin()->second->ConnectDestroyed(), connections_.erase(connections_.begin());
}

void TcpServer::SetConnectionCallback(TcpConnection::ConnectionCallback callback) {
    connection_callback_ = std::move(callback);
}
void TcpServer::SetMessageCallback(TcpConnection::MessageCallback callback) {
    message_callback_ = std::move(callback);
}
void TcpServer::SetTrafficCallback(TcpConnection::TrafficCallback callback) {
    traffic_callback_ = std::move(callback);
}

void TcpServer::Start() {
    loop_->AssertInLoopThread();
    if (started_) return;
    listen_socket_.Listen();
    listen_channel_->EnableReading();
    started_ = true;
    LOG_INFO("listening on " + address_ + ':' + std::to_string(port_));
}

std::size_t TcpServer::ConnectionCount() const noexcept { return connections_.size(); }

void TcpServer::HandleAccept() {
    while (true) {
        std::string address;
        std::uint16_t port = 0;
        const int client_fd = listen_socket_.Accept(&address, &port);
        if (client_fd < 0) break;
        if (connections_.size() >= options_.max_clients) {
            LOG_WARN("connection limit reached; rejecting " + address + ':' +
                     std::to_string(port));
            ::close(client_fd);
            continue;
        }
        auto connection = std::make_shared<TcpConnection>(
            loop_, client_fd, address + ':' + std::to_string(port), options_.connection);
        connection->SetConnectionCallback(connection_callback_);
        connection->SetMessageCallback(message_callback_);
        connection->SetTrafficCallback(traffic_callback_);
        connection->SetCloseCallback([this](const TcpConnection::Ptr& closed) {
            RemoveConnection(closed);
        });
        connections_.emplace(client_fd, connection);
        connection->ConnectEstablished();
    }
}

void TcpServer::RemoveConnection(const TcpConnection::Ptr& connection) {
    loop_->AssertInLoopThread();
    const auto found = connections_.find(connection->Fd());
    if (found == connections_.end()) return;
    auto retained = found->second;
    connections_.erase(found);
    retained->ConnectDestroyed();
}

}  // namespace cachefly::net
