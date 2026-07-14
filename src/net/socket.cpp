#include "cachefly/net/socket.h"

#include <cerrno>
#include <stdexcept>
#include <system_error>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace cachefly::net {

Socket::Socket(int fd) noexcept : fd_(fd) {}
Socket::~Socket() { if (fd_ >= 0) ::close(fd_); }
Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

Socket Socket::CreateNonBlocking() {
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "socket");
    return Socket(fd);
}

int Socket::Fd() const noexcept { return fd_; }

void Socket::SetReuseAddress(bool enabled) {
    const int value = enabled ? 1 : 0;
    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
        throw std::system_error(errno, std::generic_category(), "setsockopt");
    }
}

void Socket::Bind(const std::string& address, std::uint16_t port) {
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(port);
    if (::inet_pton(AF_INET, address.c_str(), &endpoint.sin_addr) != 1) {
        throw std::invalid_argument("invalid IPv4 address: " + address);
    }
    if (::bind(fd_, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) < 0) {
        throw std::system_error(errno, std::generic_category(), "bind");
    }
}

void Socket::Listen(int backlog) {
    if (::listen(fd_, backlog) < 0) throw std::system_error(errno, std::generic_category(), "listen");
}

int Socket::Accept(std::string* peer_address, std::uint16_t* peer_port) {
    sockaddr_in peer{};
    socklen_t length = sizeof(peer);
    const int client = ::accept4(fd_, reinterpret_cast<sockaddr*>(&peer), &length,
                                 SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNABORTED) return -1;
        throw std::system_error(errno, std::generic_category(), "accept4");
    }
    char address[INET_ADDRSTRLEN]{};
    *peer_address = ::inet_ntop(AF_INET, &peer.sin_addr, address, sizeof(address)) != nullptr
                        ? address : "unknown";
    *peer_port = ntohs(peer.sin_port);
    return client;
}

}  // namespace cachefly::net
