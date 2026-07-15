#include "cachefly/http/admin_server.h"

#include <sstream>
#include <utility>

#include "cachefly/metrics/metrics.h"
#include "cachefly/net/buffer.h"
#include "cachefly/net/tcp_connection.h"

namespace cachefly::http {
namespace {

net::TcpServerOptions AdminServerOptions() {
    net::TcpServerOptions options;
    options.max_clients = 128;
    options.connection.max_request_bytes = 64ULL * 1024ULL;
    options.connection.max_output_bytes = 1ULL * 1024ULL * 1024ULL;
    return options;
}

}  // namespace

AdminServer::AdminServer(net::EventLoop* loop,
                         const std::string& address,
                         std::uint16_t port,
                         metrics::Metrics* metrics,
                         JsonCallback status,
                         JsonCallback config)
    : metrics_(metrics),
      status_callback_(std::move(status)),
      config_callback_(std::move(config)),
      server_(loop, address, port, AdminServerOptions()) {
    server_.SetMessageCallback([this](const net::TcpConnection::Ptr& connection,
                                      net::Buffer* input) {
        const std::string_view data = input->View();
        const std::size_t headers_end = data.find("\r\n\r\n");
        if (headers_end == std::string_view::npos) return;
        const std::size_t line_end = data.find("\r\n");
        const std::string_view line = data.substr(0, line_end);
        const std::size_t first_space = line.find(' ');
        const std::size_t second_space = line.find(' ', first_space + 1);
        std::string response;
        if (first_space == std::string_view::npos || second_space == std::string_view::npos ||
            line.substr(0, first_space) != "GET") {
            response = HttpResponse(400, "text/plain", "bad request\n");
        } else {
            response = ResponseForPath(std::string(line.substr(
                first_space + 1, second_space - first_space - 1)));
        }
        input->Retrieve(headers_end + 4);
        connection->Send(std::move(response));
        connection->Shutdown();
    });
}

void AdminServer::Start() { server_.Start(); }

std::string AdminServer::ResponseForPath(const std::string& path) const {
    if (path == "/metrics") return HttpResponse(200, "text/plain; version=0.0.4", metrics_->Prometheus());
    if (path == "/status") return HttpResponse(200, "application/json", status_callback_());
    if (path == "/config") return HttpResponse(200, "application/json", config_callback_());
    return HttpResponse(404, "text/plain", "not found\n");
}

std::string AdminServer::HttpResponse(int status,
                                      const std::string& content_type,
                                      const std::string& body) {
    const char* reason = status == 200 ? "OK" : status == 400 ? "Bad Request" : "Not Found";
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n" << body;
    return response.str();
}

}  // namespace cachefly::http
