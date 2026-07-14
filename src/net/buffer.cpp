#include "cachefly/net/buffer.h"

#include <algorithm>
#include <cerrno>
#include <stdexcept>

#include <sys/uio.h>
#include <unistd.h>

namespace cachefly::net {

Buffer::Buffer(std::size_t initial_size) : storage_(initial_size) {}
std::size_t Buffer::ReadableBytes() const noexcept { return writer_index_ - reader_index_; }
std::size_t Buffer::WritableBytes() const noexcept { return storage_.size() - writer_index_; }
const char* Buffer::Peek() const noexcept { return storage_.data() + reader_index_; }
std::string_view Buffer::View() const noexcept { return {Peek(), ReadableBytes()}; }

void Buffer::Retrieve(std::size_t length) {
    if (length > ReadableBytes()) throw std::out_of_range("buffer retrieve exceeds readable bytes");
    if (length == ReadableBytes()) RetrieveAll();
    else reader_index_ += length;
}

void Buffer::RetrieveAll() noexcept { reader_index_ = 0; writer_index_ = 0; }

std::string Buffer::RetrieveAllAsString() {
    std::string result(View());
    RetrieveAll();
    return result;
}

void Buffer::Append(std::string_view data) {
    EnsureWritable(data.size());
    std::copy(data.begin(), data.end(),
              storage_.begin() + static_cast<std::ptrdiff_t>(writer_index_));
    writer_index_ += data.size();
}

long Buffer::ReadFd(int fd, int* saved_errno) {
    char extra[65536];
    iovec vectors[2];
    const std::size_t writable = WritableBytes();
    vectors[0].iov_base = storage_.data() + writer_index_;
    vectors[0].iov_len = writable;
    vectors[1].iov_base = extra;
    vectors[1].iov_len = sizeof(extra);
    const int vector_count = writable < sizeof(extra) ? 2 : 1;
    const ssize_t count = ::readv(fd, vectors, vector_count);
    if (count < 0) {
        *saved_errno = errno;
        return -1;
    }
    const std::size_t bytes = static_cast<std::size_t>(count);
    if (bytes <= writable) writer_index_ += bytes;
    else {
        writer_index_ = storage_.size();
        Append(std::string_view(extra, bytes - writable));
    }
    return static_cast<long>(count);
}

long Buffer::WriteFd(int fd, int* saved_errno) {
    const ssize_t count = ::write(fd, Peek(), ReadableBytes());
    if (count < 0) {
        *saved_errno = errno;
        return -1;
    }
    Retrieve(static_cast<std::size_t>(count));
    return static_cast<long>(count);
}

void Buffer::EnsureWritable(std::size_t length) {
    if (WritableBytes() >= length) return;
    if (reader_index_ + WritableBytes() >= length) {
        const std::size_t readable = ReadableBytes();
        std::move(storage_.begin() + static_cast<std::ptrdiff_t>(reader_index_),
                  storage_.begin() + static_cast<std::ptrdiff_t>(writer_index_),
                  storage_.begin());
        reader_index_ = 0;
        writer_index_ = readable;
    } else {
        storage_.resize(writer_index_ + length);
    }
}

}  // namespace cachefly::net
