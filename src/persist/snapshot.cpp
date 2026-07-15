#include "cachefly/persist/snapshot.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

#include "cachefly/resp/resp_value.h"

namespace cachefly::persist {
namespace {

class UniqueFd {
public:
    explicit UniqueFd(int fd) : fd_(fd) {}
    ~UniqueFd() { if (fd_ >= 0) ::close(fd_); }
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    [[nodiscard]] int Get() const noexcept { return fd_; }
    void Close() noexcept {
        if (fd_ >= 0) ::close(std::exchange(fd_, -1));
    }

private:
    int fd_;
};

[[noreturn]] void ThrowSystemError(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}

void WriteAll(int fd, const std::string& data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written = ::write(fd, data.data() + offset, data.size() - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            ThrowSystemError("write snapshot");
        }
        if (written == 0) throw std::runtime_error("snapshot write made no progress");
        offset += static_cast<std::size_t>(written);
    }
}

}  // namespace

void Snapshot::Save(const std::string& path,
                    const std::vector<storage::SnapshotEntry>& entries) {
    const std::filesystem::path destination(path);
    const std::filesystem::path directory = destination.has_parent_path()
                                                ? destination.parent_path()
                                                : std::filesystem::path(".");
    const std::string temporary = path + ".tmp";
    std::error_code cleanup_error;
    try {
        UniqueFd directory_fd(::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        if (directory_fd.Get() < 0) ThrowSystemError("open snapshot directory");
        UniqueFd output(::open(temporary.c_str(),
                               O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644));
        if (output.Get() < 0) ThrowSystemError("open temporary snapshot");
        for (const auto& entry : entries) {
            std::vector<std::string> command{"SET", entry.key, entry.value};
            if (entry.ttl.has_value()) {
                command.push_back("PX");
                command.push_back(std::to_string(
                    std::max<std::int64_t>(1, entry.ttl->count())));
            }
            WriteAll(output.Get(), resp::EncodeCommand(command));
        }
        if (::fdatasync(output.Get()) < 0) ThrowSystemError("fdatasync snapshot");
        output.Close();
        if (::fsync(directory_fd.Get()) < 0) ThrowSystemError("fsync snapshot directory");
        if (::rename(temporary.c_str(), path.c_str()) < 0) {
            ThrowSystemError("replace snapshot");
        }
        if (::fsync(directory_fd.Get()) < 0) ThrowSystemError("fsync replaced snapshot");
    } catch (...) {
        std::filesystem::remove(temporary, cleanup_error);
        throw;
    }
}

}  // namespace cachefly::persist
