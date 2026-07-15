#include "cachefly/config/config.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace cachefly {
namespace {

std::string Trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::string Lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return text;
}

bool ParseBool(const std::string& text) {
    const std::string value = Lowercase(Trim(text));
    if (value == "true" || value == "yes" || value == "on" || value == "1") return true;
    if (value == "false" || value == "no" || value == "off" || value == "0") return false;
    throw std::invalid_argument("invalid boolean value: " + text);
}

std::size_t ParseUnsigned(const std::string& text, const std::string& key) {
    const std::string value = Trim(text);
    if (value.empty() || value.front() == '-') {
        throw std::invalid_argument("invalid integer for " + key + ": " + text);
    }
    std::size_t consumed = 0;
    unsigned long long parsed = 0;
    try {
        parsed = std::stoull(value, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid integer for " + key + ": " + text);
    }
    if (consumed != value.size() || parsed > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("invalid integer for " + key + ": " + text);
    }
    return static_cast<std::size_t>(parsed);
}

std::uint16_t ParsePort(const std::string& text, const std::string& key) {
    const std::size_t value = ParseUnsigned(text, key);
    if (value == 0 || value > 65535) {
        throw std::invalid_argument(key + " must be in range 1..65535");
    }
    return static_cast<std::uint16_t>(value);
}

std::pair<std::string, std::string> ParseOption(std::string_view argument) {
    if (!argument.starts_with("--")) {
        throw std::invalid_argument("unexpected positional argument: " + std::string(argument));
    }
    const std::size_t equals = argument.find('=');
    if (equals == std::string_view::npos || equals == 2 || equals + 1 == argument.size()) {
        throw std::invalid_argument("expected --key=value, got: " + std::string(argument));
    }
    return {std::string(argument.substr(2, equals - 2)),
            std::string(argument.substr(equals + 1))};
}

void AppendJsonString(std::ostringstream& output, std::string_view value) {
    static constexpr char kHex[] = "0123456789abcdef";
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20) {
                    output << "\\u00" << kHex[character >> 4] << kHex[character & 0x0f];
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    output << '"';
}

}  // namespace

ServerConfig ConfigLoader::LoadFromFile(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("cannot open config file: " + path);
    }
    ServerConfig config;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const std::string normalized = Trim(line);
        if (normalized.empty() || normalized.front() == '#') continue;
        const std::size_t equals = normalized.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("invalid config line " + std::to_string(line_number));
        }
        const std::string key = Trim(std::string_view(normalized).substr(0, equals));
        const std::string value = Trim(std::string_view(normalized).substr(equals + 1));
        try {
            Apply(config, key, value);
        } catch (const std::exception& error) {
            throw std::runtime_error("invalid config line " + std::to_string(line_number) +
                                     ": " + error.what());
        }
    }
    Validate(config);
    return config;
}

ServerConfig ConfigLoader::LoadFromArgs(int argc, char* argv[]) {
    std::string config_path;
    for (int index = 1; index < argc; ++index) {
        const auto [key, value] = ParseOption(argv[index]);
        if (key == "config") {
            if (!config_path.empty()) {
                throw std::invalid_argument("--config may only be specified once");
            }
            config_path = value;
        }
    }
    ServerConfig config = config_path.empty() ? ServerConfig{} : LoadFromFile(config_path);
    for (int index = 1; index < argc; ++index) {
        const auto [key, value] = ParseOption(argv[index]);
        if (key != "config") Apply(config, key, value);
    }
    Validate(config);
    return config;
}

std::size_t ConfigLoader::ParseMemorySize(const std::string& text) {
    const std::string value = Lowercase(Trim(text));
    if (value.empty() || value.front() == '-') {
        throw std::invalid_argument("invalid memory size: " + text);
    }
    std::size_t consumed = 0;
    long double number = 0;
    try {
        number = std::stold(value, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid memory size: " + text);
    }
    if (!std::isfinite(number) || number < 0) {
        throw std::invalid_argument("invalid memory size: " + text);
    }
    const std::string unit = Trim(std::string_view(value).substr(consumed));
    long double multiplier = 1;
    if (unit.empty() || unit == "b") multiplier = 1;
    else if (unit == "k" || unit == "kb" || unit == "kib") multiplier = 1024;
    else if (unit == "m" || unit == "mb" || unit == "mib") multiplier = 1024ULL * 1024ULL;
    else if (unit == "g" || unit == "gb" || unit == "gib") multiplier = 1024ULL * 1024ULL * 1024ULL;
    else throw std::invalid_argument("unknown memory unit: " + unit);

    const long double bytes = number * multiplier;
    if (bytes > static_cast<long double>(std::numeric_limits<std::size_t>::max())) {
        throw std::overflow_error("memory size is too large: " + text);
    }
    return static_cast<std::size_t>(bytes);
}

std::string ConfigLoader::ToJson(const ServerConfig& config) {
    std::ostringstream output;
    output << "{\"bind\":";
    AppendJsonString(output, config.bind_address);
    output << ",\"port\":" << config.port
           << ",\"shard_threads\":" << config.shard_threads
           << ",\"max_clients\":" << config.max_clients
           << ",\"max_request_bytes\":" << config.max_request_bytes
           << ",\"max_output_bytes\":" << config.max_output_bytes
           << ",\"maxmemory_bytes\":" << config.maxmemory_bytes
           << ",\"eviction_policy\":";
    AppendJsonString(output, config.eviction_policy);
    output << ",\"log_level\":";
    AppendJsonString(output, config.log_level);
    output << ",\"log_file\":";
    AppendJsonString(output, config.log_file);
    output << ",\"appendonly\":" << (config.appendonly ? "true" : "false")
           << ",\"appendfilename\":";
    AppendJsonString(output, config.appendfilename);
    output << ",\"appendfsync\":";
    AppendJsonString(output, config.appendfsync);
    output << ",\"snapshot\":" << (config.snapshot ? "true" : "false")
           << ",\"snapshotfilename\":";
    AppendJsonString(output, config.snapshotfilename);
    output << ",\"admin_port\":" << config.admin_port << '}';
    return output.str();
}

void ConfigLoader::Validate(const ServerConfig& config) {
    if (config.bind_address.empty()) throw std::invalid_argument("bind must not be empty");
    if (config.shard_threads == 0) throw std::invalid_argument("shard_threads must be positive");
    if (config.max_clients == 0) throw std::invalid_argument("max_clients must be positive");
    if (config.max_request_bytes == 0) throw std::invalid_argument("max_request_bytes must be positive");
    if (config.max_output_bytes == 0) throw std::invalid_argument("max_output_bytes must be positive");
    if (config.maxmemory_bytes == 0) throw std::invalid_argument("maxmemory must be positive");
    static const std::unordered_set<std::string> evictions{"lru", "lfu", "random", "noeviction"};
    static const std::unordered_set<std::string> levels{"trace", "debug", "info", "warn", "warning", "error", "fatal"};
    static const std::unordered_set<std::string> fsyncs{"always", "everysec", "no"};
    if (!evictions.contains(config.eviction_policy)) throw std::invalid_argument("invalid eviction_policy");
    if (!levels.contains(config.log_level)) throw std::invalid_argument("invalid log_level");
    if (!fsyncs.contains(config.appendfsync)) throw std::invalid_argument("invalid appendfsync");
    if (config.appendfilename.empty() || config.snapshotfilename.empty()) {
        throw std::invalid_argument("persistence filenames must not be empty");
    }
}

void ConfigLoader::Apply(ServerConfig& config,
                         const std::string& key,
                         const std::string& value) {
    if (key == "bind") config.bind_address = value;
    else if (key == "port") config.port = ParsePort(value, key);
    else if (key == "shard_threads") config.shard_threads = ParseUnsigned(value, key);
    else if (key == "max_clients") config.max_clients = ParseUnsigned(value, key);
    else if (key == "max_request_bytes") config.max_request_bytes = ParseMemorySize(value);
    else if (key == "max_output_bytes") config.max_output_bytes = ParseMemorySize(value);
    else if (key == "maxmemory") config.maxmemory_bytes = ParseMemorySize(value);
    else if (key == "eviction_policy") config.eviction_policy = Lowercase(Trim(value));
    else if (key == "log_level") config.log_level = Lowercase(Trim(value));
    else if (key == "log_file") config.log_file = value;
    else if (key == "appendonly") config.appendonly = ParseBool(value);
    else if (key == "appendfilename") config.appendfilename = value;
    else if (key == "appendfsync") config.appendfsync = Lowercase(Trim(value));
    else if (key == "snapshot") config.snapshot = ParseBool(value);
    else if (key == "snapshotfilename") config.snapshotfilename = value;
    else if (key == "admin_port") config.admin_port = ParsePort(value, key);
    else throw std::invalid_argument("unknown config key: " + key);
}

}  // namespace cachefly
