#include <atomic>
#include <cerrno>
#include <memory>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

#include "cachefly/net/buffer.h"
#include "cachefly/net/event_loop.h"
#include "cachefly/net/tcp_connection.h"
#include "test_harness.h"

TEST_CASE("buffer append and retrieve") {
    cachefly::net::Buffer buffer(4);
    buffer.Append("hello");
    EXPECT_EQ(buffer.ReadableBytes(), 5U);
    EXPECT_EQ(buffer.View(), "hello");
    buffer.Retrieve(2);
    EXPECT_EQ(buffer.RetrieveAllAsString(), "llo");
    EXPECT_EQ(buffer.ReadableBytes(), 0U);
}

TEST_CASE("buffer reads a large socket payload") {
    int sockets[2]{};
    EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
    const std::string payload(10000, 'x');
    EXPECT_EQ(::write(sockets[0], payload.data(), payload.size()),
              static_cast<ssize_t>(payload.size()));
    cachefly::net::Buffer buffer(8);
    int saved_errno = 0;
    EXPECT_EQ(buffer.ReadFd(sockets[1], &saved_errno), static_cast<long>(payload.size()));
    EXPECT_EQ(buffer.View(), payload);
    ::close(sockets[0]);
    ::close(sockets[1]);
}

TEST_CASE("event loop accepts cross thread work") {
    cachefly::net::EventLoop loop;
    std::atomic<bool> executed{false};
    std::thread producer([&loop, &executed] {
        loop.QueueInLoop([&loop, &executed] {
            executed.store(true);
            loop.Quit();
        });
    });
    loop.Loop();
    producer.join();
    EXPECT_TRUE(executed.load());
}

TEST_CASE("connection closes before dispatching oversized input") {
    int sockets[2]{};
    EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
    cachefly::net::EventLoop loop;
    cachefly::net::TcpConnectionOptions options;
    options.max_request_bytes = 4;
    auto connection = std::make_shared<cachefly::net::TcpConnection>(
        &loop, sockets[1], "test-input", options);
    bool dispatched = false;
    bool closed = false;
    connection->SetMessageCallback(
        [&dispatched](const auto&, cachefly::net::Buffer*) { dispatched = true; });
    connection->SetCloseCallback([&loop, &closed](const auto&) {
        closed = true;
        loop.Quit();
    });
    connection->ConnectEstablished();
    EXPECT_EQ(::write(sockets[0], "12345", 5), 5);
    loop.Loop();
    EXPECT_TRUE(closed);
    EXPECT_TRUE(!dispatched);
    connection->ConnectDestroyed();
    connection.reset();
    ::close(sockets[0]);
}

TEST_CASE("connection rejects output above its outstanding byte limit") {
    int sockets[2]{};
    EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
    cachefly::net::EventLoop loop;
    cachefly::net::TcpConnectionOptions options;
    options.max_output_bytes = 4;
    auto connection = std::make_shared<cachefly::net::TcpConnection>(
        &loop, sockets[1], "test-output", options);
    bool closed = false;
    connection->SetCloseCallback([&closed](const auto&) { closed = true; });
    connection->ConnectEstablished();
    connection->Send("12345");
    EXPECT_TRUE(closed);
    EXPECT_TRUE(!connection->Connected());
    connection->ConnectDestroyed();
    connection.reset();
    ::close(sockets[0]);
}
