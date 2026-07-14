#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cachefly::test {

struct TestCase {
    std::string name;
    std::function<void()> function;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

class Registrar {
public:
    Registrar(std::string name, std::function<void()> function) {
        Registry().push_back({std::move(name), std::move(function)});
    }
};

template <typename Exception, typename Function>
void ExpectThrow(Function&& function, const char* expression) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return;
    } catch (...) {
        throw std::runtime_error(std::string(expression) + " threw the wrong exception");
    }
    throw std::runtime_error(std::string(expression) + " did not throw");
}

}  // namespace cachefly::test

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)
#define TEST_CASE(name)                                                        \
    static void TEST_CONCAT(TestFunction_, __LINE__)();                        \
    static ::cachefly::test::Registrar TEST_CONCAT(TestRegistrar_, __LINE__)(  \
        (name), TEST_CONCAT(TestFunction_, __LINE__));                         \
    static void TEST_CONCAT(TestFunction_, __LINE__)()
#define EXPECT_TRUE(condition)                                                  \
    do {                                                                        \
        if (!(condition)) {                                                      \
            throw std::runtime_error(std::string("expectation failed: ") +      \
                                     #condition + " at " + __FILE__ + ":" +    \
                                     std::to_string(__LINE__));                 \
        }                                                                       \
    } while (false)
#define EXPECT_EQ(actual, expected) EXPECT_TRUE((actual) == (expected))
#define EXPECT_THROW(expression, exception_type)                                \
    ::cachefly::test::ExpectThrow<exception_type>(                              \
        [&]() { static_cast<void>(expression); }, #expression)

