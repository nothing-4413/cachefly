find_package(Threads REQUIRED)

add_library(cachefly_base
    base/logger.cpp
    config/config.cpp
)

add_library(cachefly_net
    net/buffer.cpp
    net/channel.cpp
    net/event_loop.cpp
    net/socket.cpp
    net/tcp_connection.cpp
    net/tcp_server.cpp
)

add_library(cachefly_resp
    resp/resp_parser.cpp
    resp/resp_value.cpp
)

target_include_directories(cachefly_base PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_base PUBLIC Threads::Threads cachefly_project_options)

target_include_directories(cachefly_net PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_net PUBLIC cachefly_base)

target_include_directories(cachefly_resp PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_resp PUBLIC cachefly_base)

add_executable(cachefly main.cpp)
target_link_libraries(cachefly PRIVATE cachefly_net cachefly_resp)
