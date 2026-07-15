#include "cachefly/persist/aof.h"

#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

#include "cachefly/base/logger.h"
#include "cachefly/resp/resp_value.h"

namespace cachefly::persist {

FsyncPolicy ParseFsyncPolicy(const std::string& value) {
    if (value == "always") return FsyncPolicy::kAlways;
    if (value == "everysec") return FsyncPolicy::kEverySecond;
    if (value == "no") return FsyncPolicy::kNever;
    throw std::invalid_argument("invalid appendfsync policy: " + value);
}

AofWriter::AofWriter(std::string path, FsyncPolicy policy)
    : path_(std::move(path)), policy_(policy) {
    fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd_ < 0) throw std::system_error(errno, std::generic_category(), "open AOF");
    worker_ = std::thread([this] { Run(); });
}

AofWriter::~AofWriter() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_one();
    if (worker_.joinable()) worker_.join();
    ::close(fd_);
}

void AofWriter::Append(const std::vector<std::string>& command) {
    auto durable = policy_ == FsyncPolicy::kAlways
                       ? std::make_shared<std::promise<void>>() : nullptr;
    std::future<void> future;
    if (durable) future = durable->get_future();
    {
        std::lock_guard lock(mutex_);
        if (stopping_) throw std::runtime_error("AOF writer is stopping");
        queue_.push_back({resp::EncodeCommand(command), durable});
    }
    condition_.notify_one();
    if (durable) future.get();
}

void AofWriter::Run() {
    auto next_sync = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    bool dirty = false;
    while (true) {
        Record record;
        {
            std::unique_lock lock(mutex_);
            condition_.wait_until(lock, next_sync,
                                  [this] { return stopping_ || !queue_.empty(); });
            if (!queue_.empty()) {
                record = std::move(queue_.front());
                queue_.pop_front();
            } else if (stopping_) {
                if (dirty && policy_ != FsyncPolicy::kNever) ::fdatasync(fd_);
                break;
            }
        }
        if (!record.data.empty()) {
            try {
                WriteRecord(record);
                dirty = true;
                if (policy_ == FsyncPolicy::kAlways) {
                    if (::fdatasync(fd_) < 0) throw std::system_error(errno, std::generic_category(), "fdatasync");
                    dirty = false;
                }
                if (record.durable) record.durable->set_value();
            } catch (...) {
                if (record.durable) record.durable->set_exception(std::current_exception());
                else LOG_ERROR("asynchronous AOF write failed");
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (policy_ == FsyncPolicy::kEverySecond && dirty && now >= next_sync) {
            if (::fdatasync(fd_) < 0) LOG_ERROR("AOF fdatasync failed");
            dirty = false;
        }
        if (now >= next_sync) next_sync = now + std::chrono::seconds(1);
    }
}

void AofWriter::WriteRecord(const Record& record) {
    std::size_t offset = 0;
    while (offset < record.data.size()) {
        const ssize_t written = ::write(fd_, record.data.data() + offset,
                                        record.data.size() - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "write AOF");
        }
        offset += static_cast<std::size_t>(written);
    }
}

}  // namespace cachefly::persist
