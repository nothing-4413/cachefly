find_package(Threads REQUIRED)

add_library(cachefly_base
    base/logger.cpp
    config/config.cpp
)

target_include_directories(cachefly_base PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cachefly_base PUBLIC Threads::Threads cachefly_project_options)

add_executable(cachefly main.cpp)
target_link_libraries(cachefly PRIVATE cachefly_base)

