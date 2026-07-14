#include <string>
#include <vector>

#include "cachefly/resp/resp_parser.h"
#include "cachefly/resp/resp_value.h"
#include "test_harness.h"

TEST_CASE("RESP values encode correctly") {
    EXPECT_EQ(cachefly::resp::Value::Simple("OK").Encode(), "+OK\r\n");
    EXPECT_EQ(cachefly::resp::Value::Error("ERR bad\ninput").Encode(),
              "-ERR bad input\r\n");
    EXPECT_EQ(cachefly::resp::Value::Integer(-2).Encode(), ":-2\r\n");
    EXPECT_EQ(cachefly::resp::Value::Bulk("abc").Encode(), "$3\r\nabc\r\n");
    EXPECT_EQ(cachefly::resp::Value::Null().Encode(), "$-1\r\n");
}

TEST_CASE("RESP command parses from an array") {
    cachefly::resp::Parser parser;
    std::size_t consumed = 0;
    std::vector<std::string> arguments;
    std::string error;
    const std::string request = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n";
    EXPECT_EQ(parser.ParseCommand(request, &consumed, &arguments, &error),
              cachefly::resp::ParseResult::kComplete);
    EXPECT_EQ(consumed, request.size());
    EXPECT_EQ(arguments.size(), 3U);
    EXPECT_EQ(arguments[2], "value");
}

TEST_CASE("RESP parser preserves incomplete frames") {
    cachefly::resp::Parser parser;
    std::size_t consumed = 99;
    cachefly::resp::Value value;
    std::string error;
    EXPECT_EQ(parser.Parse("$5\r\nhel", &consumed, &value, &error),
              cachefly::resp::ParseResult::kIncomplete);
    EXPECT_EQ(consumed, 0U);
    EXPECT_TRUE(error.empty());
}

TEST_CASE("RESP parser consumes one pipelined frame") {
    cachefly::resp::Parser parser;
    std::size_t consumed = 0;
    cachefly::resp::Value value;
    std::string error;
    const std::string pipeline = "+OK\r\n:42\r\n";
    EXPECT_EQ(parser.Parse(pipeline, &consumed, &value, &error),
              cachefly::resp::ParseResult::kComplete);
    EXPECT_EQ(consumed, 5U);
    EXPECT_EQ(value.type, cachefly::resp::Type::kSimpleString);
}

TEST_CASE("RESP parser rejects malformed input and limits") {
    cachefly::resp::Parser parser(4, 2, 2);
    std::size_t consumed = 0;
    cachefly::resp::Value value;
    std::string error;
    EXPECT_EQ(parser.Parse("$5\r\nhello\r\n", &consumed, &value, &error),
              cachefly::resp::ParseResult::kError);
    EXPECT_TRUE(!error.empty());
    EXPECT_EQ(parser.Parse("!bad\r\n", &consumed, &value, &error),
              cachefly::resp::ParseResult::kError);
}
