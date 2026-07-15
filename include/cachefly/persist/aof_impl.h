#pragma once

#include <fstream>
#include <filesystem>
#include <iterator>
#include <stdexcept>

#include "cachefly/resp/resp_parser.h"

namespace cachefly::persist {

template <typename Callback>
std::size_t AofWriter::Replay(const std::string& path,
                              Callback&& callback,
                              bool truncate_incomplete) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return 0;
    const std::string data((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    input.close();
    resp::Parser parser;
    std::size_t offset = 0;
    std::size_t count = 0;
    while (offset < data.size()) {
        std::size_t consumed = 0;
        std::vector<std::string> command;
        std::string error;
        const auto result = parser.ParseCommand(
            std::string_view(data).substr(offset), &consumed, &command, &error);
        if (result == resp::ParseResult::kIncomplete) {
            if (truncate_incomplete) std::filesystem::resize_file(path, offset);
            break;
        }
        if (result == resp::ParseResult::kError) {
            throw std::runtime_error("corrupt AOF at byte " + std::to_string(offset) +
                                     ": " + error);
        }
        callback(command);
        offset += consumed;
        ++count;
    }
    return count;
}

}  // namespace cachefly::persist
