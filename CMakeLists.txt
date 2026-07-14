cmake_minimum_required(VERSION 3.16)

project(cachefly VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CACHEFLY_BUILD_TESTS "Build unit tests" ON)
option(CACHEFLY_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(CACHEFLY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(cachefly_project_options INTERFACE)
if(MSVC)
    target_compile_options(cachefly_project_options INTERFACE /W4 /permissive-)
    if(CACHEFLY_WARNINGS_AS_ERRORS)
        target_compile_options(cachefly_project_options INTERFACE /WX)
    endif()
else()
    target_compile_options(cachefly_project_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    if(CACHEFLY_WARNINGS_AS_ERRORS)
        target_compile_options(cachefly_project_options INTERFACE -Werror)
    endif()
endif()

if(CACHEFLY_ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cachefly_project_options INTERFACE
            -fsanitize=address -fno-omit-frame-pointer
        )
        target_link_options(cachefly_project_options INTERFACE -fsanitize=address)
    else()
        message(FATAL_ERROR "CACHEFLY_ENABLE_ASAN requires GCC or Clang")
    endif()
endif()

add_subdirectory(src)

if(CACHEFLY_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

