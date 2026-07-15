#include <string>

#include "cachefly/metrics/metrics.h"
#include "test_harness.h"

TEST_CASE("metrics export counters and latency histogram") {
    cachefly::metrics::Metrics metrics;
    metrics.ConnectionOpened();
    metrics.AddBytes(10, 20);
    metrics.CacheHit();
    metrics.ObserveCommand(75, false);
    const std::string output = metrics.Prometheus();
    EXPECT_TRUE(output.find("cachefly_commands_total 1") != std::string::npos);
    EXPECT_TRUE(output.find("cachefly_connections 1") != std::string::npos);
    EXPECT_TRUE(output.find("le=\"100\"} 1") != std::string::npos);
    metrics.ConnectionClosed();
    EXPECT_EQ(metrics.ActiveConnections(), 0U);
}
