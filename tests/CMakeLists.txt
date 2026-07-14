add_executable(cachefly_tests
    test_config.cpp
    test_command.cpp
    test_logger.cpp
    test_main.cpp
    test_net.cpp
    test_resp.cpp
)
target_link_libraries(cachefly_tests PRIVATE cachefly_net cachefly_command)
add_test(NAME cachefly.unit COMMAND cachefly_tests)
