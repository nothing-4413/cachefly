#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "cachefly/base/noncopyable.h"
#include "cachefly/net/tcp_server.h"

namespace cachefly::metrics { class Metrics; }
namespace cachefly::net { class EventLoop; }

namespace cachefly::http {

class AdminServer final : public cachefly::NonCopyable {
public:
    using JsonCallback = std::function<std::string()>;

    AdminServer(net::EventLoop* loop,
                const std::string& address,
                std::uint16_t port,
                metrics::Metrics* metrics,
                JsonCallback status,
                JsonCallback config);
    void Start();

    [[nodiscard]] std::string ResponseForPath(const std::string& path) const;

private:
    static std::string HttpResponse(int status,
                                    const std::string& content_type,
                                    const std::string& body);

    metrics::Metrics* metrics_;
    JsonCallback status_callback_;
    JsonCallback config_callback_;
    net::TcpServer server_;
};

}  // namespace cachefly::http
