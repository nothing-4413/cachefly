#include "cachefly/persist/snapshot.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "cachefly/resp/resp_value.h"

namespace cachefly::persist {

void Snapshot::Save(const std::string& path,
                    const std::vector<storage::SnapshotEntry>& entries) {
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) throw std::runtime_error("cannot open snapshot: " + temporary);
    for (const auto& entry : entries) {
        std::vector<std::string> command{"SET", entry.key, entry.value};
        if (entry.ttl.has_value()) {
            command.push_back("PX");
            command.push_back(std::to_string(std::max<std::int64_t>(1, entry.ttl->count())));
        }
        output << resp::EncodeCommand(command);
    }
    output.flush();
    if (!output) throw std::runtime_error("failed to write snapshot: " + temporary);
    output.close();
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) throw std::runtime_error("cannot replace snapshot: " + error.message());
}

}  // namespace cachefly::persist
