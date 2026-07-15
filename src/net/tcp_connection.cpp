#include "cachefly/net/tcp_connection.h"

#include <cerrno>
#include <cstring>
#include <utility>

#include <sys/socket.h>
#include <unistd.h>

#include "cachefly/base/logger.h"
#include "cachefly/net/channel.h"
#include "cachefly/net/event_loop.h"

namespace cachefly::net {

TcpConnection::TcpConnection(EventLoop* loop, int fd, std::string peer)
    : loop_(loop), fd_(fd), peer_(std::move(peer)),
      channel_(std::make_unique<Channel>(loop, fd)) {
    channel_->SetReadCallback([this] { HandleRead(); });
    channel_->SetWriteCallback([this] { HandleWrite(); });
    channel_->SetCloseCallback([this] { HandleClose(); });
    channel_->SetErrorCallback([this] { HandleError(); });
}

TcpConnection::~TcpConnection() { ::close(fd_); }
int TcpConnection::Fd() const noexcept { return fd_; }
const std::string& TcpConnection::Peer() const noexcept { return peer_; }
bool TcpConnection::Connected() const noexcept { return state_.load() == State::kConnected; }

void TcpConnection::Send(std::string message) {
    if (!Connected()) return;
    auto self = shared_from_this();
    loop_->RunInLoop([self, message = std::move(message)]() mutable {
        self->SendInLoop(std::move(message));
    });
}

void TcpConnection::Shutdown() {
    State expected = State::kConnected;
    if (state_.compare_exchange_strong(expected, State::kDisconnecting)) {
        auto self = shared_from_this();
        loop_->RunInLoop([self] { self->ShutdownInLoop(); });
    }
}

void TcpConnection::SetConnectionCallback(ConnectionCallback callback) { connection_callback_ = std::move(callback); }
void TcpConnection::SetMessageCallback(MessageCallback callback) { message_callback_ = std::move(callback); }
void TcpConnection::SetCloseCallback(CloseCallback callback) { close_callback_ = std::move(callback); }
void TcpConnection::SetTrafficCallback(TrafficCallback callback) { traffic_callback_ = std::move(callback); }

void TcpConnection::ConnectEstablished() {
    loop_->AssertInLoopThread();
    state_.store(State::kConnected);
    channel_->EnableReading();
    if (connection_callback_) connection_callback_(shared_from_this());
}

void TcpConnection::ConnectDestroyed() {
    loop_->AssertInLoopThread();
    if (state_.exchange(State::kDisconnected) != State::kDisconnected) {
        channel_->DisableAll();
        if (connection_callback_) connection_callback_(shared_from_this());
    }
    channel_->Remove();
}

void TcpConnection::HandleRead() {
    auto guard = shared_from_this();
    int saved_errno = 0;
    const long count = input_buffer_.ReadFd(fd_, &saved_errno);
    if (count > 0) {
        if (traffic_callback_) traffic_callback_(static_cast<std::size_t>(count), 0);
        if (message_callback_) message_callback_(guard, &input_buffer_);
    }
    else if (count == 0) HandleClose();
    else if (count < 0 && saved_errno != EAGAIN && saved_errno != EWOULDBLOCK) {
        HandleError();
        HandleClose();
    }
}

void TcpConnection::HandleWrite() {
    if (!channel_->IsWriting()) return;
    int saved_errno = 0;
    const long count = output_buffer_.WriteFd(fd_, &saved_errno);
    if (count > 0) {
        if (traffic_callback_) traffic_callback_(0, static_cast<std::size_t>(count));
        if (output_buffer_.ReadableBytes() == 0) {
            channel_->DisableWriting();
            if (state_.load() == State::kDisconnecting) ShutdownInLoop();
        }
    } else if (count < 0 && saved_errno != EAGAIN && saved_errno != EWOULDBLOCK) {
        HandleError();
    }
}

void TcpConnection::HandleClose() {
    auto guard = shared_from_this();
    if (state_.exchange(State::kDisconnected) == State::kDisconnected) return;
    channel_->DisableAll();
    if (connection_callback_) connection_callback_(guard);
    if (close_callback_) close_callback_(guard);
}

void TcpConnection::HandleError() {
    int socket_error = 0;
    socklen_t length = sizeof(socket_error);
    ::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &socket_error, &length);
    LOG_ERROR("connection error for " + peer_ + ": " + std::strerror(socket_error));
}

void TcpConnection::SendInLoop(std::string message) {
    loop_->AssertInLoopThread();
    if (state_.load() == State::kDisconnected) return;
    std::size_t sent = 0;
    if (!channel_->IsWriting() && output_buffer_.ReadableBytes() == 0) {
        const ssize_t count = ::send(fd_, message.data(), message.size(), MSG_NOSIGNAL);
        if (count >= 0) {
            sent = static_cast<std::size_t>(count);
            if (traffic_callback_ && sent > 0) traffic_callback_(0, sent);
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            HandleError();
            return;
        }
    }
    if (sent < message.size()) {
        output_buffer_.Append(std::string_view(message).substr(sent));
        if (!channel_->IsWriting()) channel_->EnableWriting();
    }
}

void TcpConnection::ShutdownInLoop() {
    loop_->AssertInLoopThread();
    if (!channel_->IsWriting()) ::shutdown(fd_, SHUT_WR);
}

}  // namespace cachefly::net
