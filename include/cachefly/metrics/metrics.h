#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cachefly::metrics {
class Metrics {
public:
    void ObserveCommand(std::uint64_t microseconds, bool error) noexcept;
    void ConnectionOpened() noexcept;
    void ConnectionClosed() noexcept;
    void AddBytes(std::size_t read, std::size_t written) noexcept;
    void CacheHit() noexcept;
    void CacheMiss() noexcept;
    void KeyExpired() noexcept;
    void KeyEvicted() noexcept;
    [[nodiscard]] std::uint64_t ActiveConnections() const noexcept;
    [[nodiscard]] std::uint64_t Commands() const noexcept;
    [[nodiscard]] std::string Prometheus() const;
private:
    static constexpr std::array<std::uint64_t, 10> kBounds{
        50, 100, 250, 500, 1000, 2500, 5000, 10000, 50000, 100000};
    std::atomic<std::uint64_t> commands_{0}, errors_{0}, active_connections_{0};
    std::atomic<std::uint64_t> total_connections_{0}, bytes_read_{0}, bytes_written_{0};
    std::atomic<std::uint64_t> hits_{0}, misses_{0}, expired_{0}, evicted_{0}, latency_sum_{0};
    std::array<std::atomic<std::uint64_t>, kBounds.size()> buckets_{};
};
}  // namespace cachefly::metrics
