cmake_minimum_required(VERSION 3.14)
project(lldb CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

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

add_library(cache STATIC util/cache.cpp)
target_link_libraries(cache PUBLIC hash)
target_include_directories(cache PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(logging STATIC util/logging.cpp)
target_include_directories(logging PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(comparator STATIC util/comparator.cpp)
target_include_directories(comparator PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(status STATIC util/status.cpp)
target_include_directories(status PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(env STATIC util/env.cpp)
target_link_libraries(env PUBLIC status)
target_include_directories(env PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(env_posix STATIC util/env_posix.cpp)
target_link_libraries(env_posix PUBLIC env Threads::Threads)
target_include_directories(env_posix PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(histogram STATIC util/histogram.cpp)
target_include_directories(histogram PUBLIC ${CMAKE_SOURCE_DIR})

add_library(filter_policy STATIC util/filter_policy.cpp)
target_include_directories(filter_policy PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_library(bloom STATIC util/bloom.cpp)
target_link_libraries(bloom PUBLIC filter_policy hash)
target_include_directories(bloom PUBLIC ${CMAKE_SOURCE_DIR})

add_library(iterator STATIC table/iterator.cpp)
target_include_directories(iterator PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(memenv STATIC helper/memenv/memenv.cpp)
target_include_directories(memenv PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(memenv PUBLIC env status)

add_library(random INTERFACE)
target_include_directories(random INTERFACE ${CMAKE_SOURCE_DIR})

add_library(skiplist INTERFACE)
target_include_directories(skiplist INTERFACE ${CMAKE_SOURCE_DIR})
target_link_libraries(skiplist INTERFACE arena random)

add_library(dbformat STATIC db/dbformat.cpp)
target_include_directories(dbformat PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(dbformat PUBLIC coding comparator filter_policy logging)

add_library(merger STATIC table/merger.cpp)
target_include_directories(merger PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(merger PUBLIC iterator comparator status)

add_library(options STATIC util/options.cpp)
target_link_libraries(options PUBLIC comparator env_posix)
target_include_directories(options PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

enable_testing()
add_subdirectory(test)
