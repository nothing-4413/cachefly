#include "cachefly/base/logger.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cachefly {
namespace {

std::string Lowercase(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

}  // namespace

Logger& Logger::Instance() {
    static Logger logger;
    return logger;
}

void Logger::SetLevel(LogLevel level) noexcept {
    level_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::Level() const noexcept {
    return level_.load(std::memory_order_relaxed);
}

bool Logger::SetOutputFile(const std::string& path) {
    std::lock_guard lock(output_mutex_);
    if (path.empty()) {
        output_file_.close();
        output_file_.clear();
        return true;
    }
    std::ofstream replacement(path, std::ios::app);
    if (!replacement.is_open()) {
        return false;
    }
    output_file_ = std::move(replacement);
    return true;
}

void Logger::Log(LogLevel level,
                 const char* file,
                 int line,
                 std::string_view message) {
    if (level < Level()) {
        return;
    }

    std::ostringstream record;
    record << '[' << Timestamp() << ']'
           << '[' << LevelName(level) << ']'
           << '[' << file << ':' << line << "] " << message << '\n';

    {
        std::lock_guard lock(output_mutex_);
        std::cerr << record.str();
        if (output_file_.is_open()) {
            output_file_ << record.str();
            output_file_.flush();
        }
    }
    if (level == LogLevel::kFatal) {
        std::abort();
    }
}

std::string_view Logger::LevelName(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::kTrace: return "TRACE";
        case LogLevel::kDebug: return "DEBUG";
        case LogLevel::kInfo: return "INFO";
        case LogLevel::kWarn: return "WARN";
        case LogLevel::kError: return "ERROR";
        case LogLevel::kFatal: return "FATAL";
    }
    return "UNKNOWN";
}

std::string Logger::Timestamp() {
    using Clock = std::chrono::system_clock;
    const auto now = Clock::now();
    const std::time_t seconds = Clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch()) %
                              1000;
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    std::ostringstream result;
    result << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.'
           << std::setfill('0') << std::setw(3) << milliseconds.count();
    return result.str();
}

LogLevel ParseLogLevel(std::string_view text) {
    const std::string value = Lowercase(text);
    if (value == "trace") return LogLevel::kTrace;
    if (value == "debug") return LogLevel::kDebug;
    if (value == "info") return LogLevel::kInfo;
    if (value == "warn" || value == "warning") return LogLevel::kWarn;
    if (value == "error") return LogLevel::kError;
    if (value == "fatal") return LogLevel::kFatal;
    throw std::invalid_argument("invalid log level: " + std::string(text));
}

}  // namespace cachefly

