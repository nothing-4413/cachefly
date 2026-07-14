#pragma once

#include <cstdint>
#include <string>

#include "cachefly/base/noncopyable.h"

namespace cachefly::net {

class Socket final : public cachefly::NonCopyable {
public:
    explicit Socket(int fd) noexcept;
    ~Socket();
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    [[nodiscard]] static Socket CreateNonBlocking();
    [[nodiscard]] int Fd() const noexcept;
    void SetReuseAddress(bool enabled);
    void Bind(const std::string& address, std::uint16_t port);
    void Listen(int backlog = 1024);
    [[nodiscard]] int Accept(std::string* peer_address, std::uint16_t* peer_port);

private:
    int fd_{-1};
};

}  // namespace cachefly::net
