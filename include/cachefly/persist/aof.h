#pragma once

#include <condition_variable>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "cachefly/base/noncopyable.h"

namespace cachefly::persist {

enum class FsyncPolicy { kAlways, kEverySecond, kNever };
[[nodiscard]] FsyncPolicy ParseFsyncPolicy(const std::string& value);

class AofWriter final : public cachefly::NonCopyable {
public:
    AofWriter(std::string path, FsyncPolicy policy);
    ~AofWriter();

    void Append(const std::vector<std::string>& command);

    template <typename Callback>
    static std::size_t Replay(const std::string& path, Callback&& callback);

private:
    struct Record {
        std::string data;
        std::shared_ptr<std::promise<void>> durable;
    };

    void Run();
    void WriteRecord(const Record& record);

    std::string path_;
    FsyncPolicy policy_;
    int fd_{-1};
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Record> queue_;
    bool stopping_{false};
    std::thread worker_;
};

}  // namespace cachefly::persist

#include "cachefly/persist/aof_impl.h"
