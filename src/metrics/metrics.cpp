#include "cachefly/metrics/metrics.h"
#include <sstream>

namespace cachefly::metrics {
void Metrics::ObserveCommand(std::uint64_t us, bool error) noexcept {
    commands_.fetch_add(1); latency_sum_.fetch_add(us); if (error) errors_.fetch_add(1);
    for (std::size_t i = 0; i < kBounds.size(); ++i) if (us <= kBounds[i]) buckets_[i].fetch_add(1);
}
void Metrics::ConnectionOpened() noexcept { active_connections_.fetch_add(1); total_connections_.fetch_add(1); }
void Metrics::ConnectionClosed() noexcept { active_connections_.fetch_sub(1); }
void Metrics::AddBytes(std::size_t read, std::size_t written) noexcept { bytes_read_.fetch_add(read); bytes_written_.fetch_add(written); }
void Metrics::CacheHit() noexcept { hits_.fetch_add(1); }
void Metrics::CacheMiss() noexcept { misses_.fetch_add(1); }
void Metrics::KeyExpired() noexcept { expired_.fetch_add(1); }
void Metrics::KeyEvicted() noexcept { evicted_.fetch_add(1); }
std::uint64_t Metrics::ActiveConnections() const noexcept { return active_connections_.load(); }
std::uint64_t Metrics::Commands() const noexcept { return commands_.load(); }
std::string Metrics::Prometheus() const {
    std::ostringstream out;
    out << "# TYPE cachefly_commands_total counter\ncachefly_commands_total " << commands_.load() << '\n'
        << "cachefly_command_errors_total " << errors_.load() << '\n'
        << "# TYPE cachefly_connections gauge\ncachefly_connections " << active_connections_.load() << '\n'
        << "cachefly_connections_total " << total_connections_.load() << '\n'
        << "cachefly_network_bytes_read_total " << bytes_read_.load() << '\n'
        << "cachefly_network_bytes_written_total " << bytes_written_.load() << '\n'
        << "cachefly_cache_hits_total " << hits_.load() << '\n'
        << "cachefly_cache_misses_total " << misses_.load() << '\n'
        << "cachefly_expired_keys_total " << expired_.load() << '\n'
        << "cachefly_evicted_keys_total " << evicted_.load() << '\n';
    for (std::size_t i = 0; i < kBounds.size(); ++i) out << "cachefly_command_latency_microseconds_bucket{le=\"" << kBounds[i] << "\"} " << buckets_[i].load() << '\n';
    out << "cachefly_command_latency_microseconds_bucket{le=\"+Inf\"} " << commands_.load() << '\n'
        << "cachefly_command_latency_microseconds_sum " << latency_sum_.load() << '\n'
        << "cachefly_command_latency_microseconds_count " << commands_.load() << '\n';
    return out.str();
}
}  // namespace cachefly::metrics
