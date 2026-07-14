#pragma once

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

#include "cachefly/base/noncopyable.h"

namespace cachefly {

enum class LogLevel { kTrace = 0, kDebug, kInfo, kWarn, kError, kFatal };

class Logger final : public NonCopyable {
public:
    static Logger& Instance();

    void SetLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel Level() const noexcept;
    [[nodiscard]] bool SetOutputFile(const std::string& path);
    void Log(LogLevel level, const char* file, int line, std::string_view message);

private:
    Logger() = default;

    [[nodiscard]] static std::string_view LevelName(LogLevel level) noexcept;
    [[nodiscard]] static std::string Timestamp();

    std::atomic<LogLevel> level_{LogLevel::kInfo};
    std::mutex output_mutex_;
    std::ofstream output_file_;
};

[[nodiscard]] LogLevel ParseLogLevel(std::string_view text);

}  // namespace cachefly

#define CACHEFLY_LOG(level, message) \
    ::cachefly::Logger::Instance().Log((level), __FILE__, __LINE__, (message))
#define LOG_TRACE(message) CACHEFLY_LOG(::cachefly::LogLevel::kTrace, (message))
#define LOG_DEBUG(message) CACHEFLY_LOG(::cachefly::LogLevel::kDebug, (message))
#define LOG_INFO(message) CACHEFLY_LOG(::cachefly::LogLevel::kInfo, (message))
#define LOG_WARN(message) CACHEFLY_LOG(::cachefly::LogLevel::kWarn, (message))
#define LOG_ERROR(message) CACHEFLY_LOG(::cachefly::LogLevel::kError, (message))
#define LOG_FATAL(message) CACHEFLY_LOG(::cachefly::LogLevel::kFatal, (message))

