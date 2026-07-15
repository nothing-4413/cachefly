#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "cachefly/storage/kv_store.h"

namespace cachefly::persist {

class Snapshot {
public:
    static void Save(const std::string& path,
                     const std::vector<storage::SnapshotEntry>& entries);

    template <typename Callback>
    static std::size_t Load(const std::string& path, Callback&& callback);
};

}  // namespace cachefly::persist

#include "cachefly/persist/snapshot_impl.h"
