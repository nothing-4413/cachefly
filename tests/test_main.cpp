#include <exception>
#include <iostream>

#include "test_harness.h"

int main() {
    std::size_t failures = 0;
    for (const auto& test : cachefly::test::Registry()) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }
    std::cout << cachefly::test::Registry().size() - failures << '/'
              << cachefly::test::Registry().size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}

