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

add_library(cachefly_metrics metrics/metrics.cpp)

add_library(cachefly_command
    command/async_dispatcher.cpp
    command/command_executor.cpp
    command/command_registry.cpp
)

add_library(cachefly_storage
    storage/kv_store.cpp
)

add_library(cachefly_shard
    shard/sharded_database.cpp
)

add_library(cachefly_persist
    persist/aof.cpp
    persist/snapshot.cpp
)

add_library(cachefly_http http/admin_server.cpp)

target_include_directories(cachefly_base PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_base PUBLIC Threads::Threads cachefly_project_options)

target_include_directories(cachefly_net PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_net PUBLIC cachefly_base)

target_include_directories(cachefly_resp PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_resp PUBLIC cachefly_base)

target_include_directories(cachefly_metrics PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_metrics PUBLIC cachefly_base)

target_include_directories(cachefly_command PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_command PUBLIC cachefly_resp cachefly_metrics)

target_include_directories(cachefly_storage PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_storage PUBLIC cachefly_command)

target_include_directories(cachefly_shard PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_shard PUBLIC cachefly_storage)

target_include_directories(cachefly_persist PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_persist PUBLIC cachefly_shard)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles([[ 
    #include <filesystem>
    int main() {
        return std::filesystem::current_path().empty() ? 1 : 0;
    }
]] CACHEFLY_STD_FILESYSTEM_LINKS)
if(NOT CACHEFLY_STD_FILESYSTEM_LINKS)
    find_library(CACHEFLY_STDCXXFS_LIBRARY stdc++fs REQUIRED)
    target_link_libraries(cachefly_persist PUBLIC ${CACHEFLY_STDCXXFS_LIBRARY})
endif()

target_include_directories(cachefly_http PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_http PUBLIC cachefly_net cachefly_metrics)

add_executable(cachefly main.cpp)
target_link_libraries(cachefly PRIVATE cachefly_net cachefly_persist cachefly_http)
