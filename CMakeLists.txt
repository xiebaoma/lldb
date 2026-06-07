cmake_minimum_required(VERSION 3.14)
project(lldb CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Libraries under test
add_library(arena STATIC util/arena.cpp)
target_include_directories(arena PUBLIC ${CMAKE_SOURCE_DIR})

add_library(coding STATIC util/coding.cpp)
target_include_directories(coding PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(hash STATIC util/hash.cpp)
target_link_libraries(hash PUBLIC coding)
target_include_directories(hash PUBLIC ${CMAKE_SOURCE_DIR})

add_library(crc32c STATIC util/crc32c.cpp)
target_link_libraries(crc32c PUBLIC coding)
target_include_directories(crc32c PUBLIC ${CMAKE_SOURCE_DIR})

add_library(bloom STATIC util/bloom.cpp)
target_link_libraries(bloom PUBLIC hash)
target_include_directories(bloom PUBLIC ${CMAKE_SOURCE_DIR})

add_library(cache STATIC util/cache.cpp)
target_link_libraries(cache PUBLIC hash)
target_include_directories(cache PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(logging STATIC util/logging.cpp)
target_include_directories(logging PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(comparator STATIC util/comparator.cpp)
target_include_directories(comparator PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

enable_testing()
add_subdirectory(test)
