#include <stdexcept>

#include "cachefly/base/logger.h"
#include "test_harness.h"

TEST_CASE("log level parsing") {
    EXPECT_EQ(cachefly::ParseLogLevel("DEBUG"), cachefly::LogLevel::kDebug);
    EXPECT_EQ(cachefly::ParseLogLevel("warning"), cachefly::LogLevel::kWarn);
    EXPECT_THROW(cachefly::ParseLogLevel("verbose"), std::invalid_argument);
}

TEST_CASE("logger level update") {
    auto& logger = cachefly::Logger::Instance();
    logger.SetLevel(cachefly::LogLevel::kError);
    EXPECT_EQ(logger.Level(), cachefly::LogLevel::kError);
    logger.SetLevel(cachefly::LogLevel::kInfo);
}

