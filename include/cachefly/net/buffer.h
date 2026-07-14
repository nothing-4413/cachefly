#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace cachefly::net {

class Buffer {
public:
    explicit Buffer(std::size_t initial_size = 4096);

    [[nodiscard]] std::size_t ReadableBytes() const noexcept;
    [[nodiscard]] std::size_t WritableBytes() const noexcept;
    [[nodiscard]] const char* Peek() const noexcept;
    [[nodiscard]] std::string_view View() const noexcept;

    void Retrieve(std::size_t length);
    void RetrieveAll() noexcept;
    [[nodiscard]] std::string RetrieveAllAsString();
    void Append(std::string_view data);

    [[nodiscard]] long ReadFd(int fd, int* saved_errno);
    [[nodiscard]] long WriteFd(int fd, int* saved_errno);

private:
    void EnsureWritable(std::size_t length);

    std::vector<char> storage_;
    std::size_t reader_index_{0};
    std::size_t writer_index_{0};
};

}  // namespace cachefly::net
