#pragma once

#include <utility>

#include "cachefly/persist/aof.h"

namespace cachefly::persist {

template <typename Callback>
std::size_t Snapshot::Load(const std::string& path, Callback&& callback) {
    return AofWriter::Replay(path, std::forward<Callback>(callback));
}

}  // namespace cachefly::persist
