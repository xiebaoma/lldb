include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

function(add_test_case name lib)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE ${lib} gtest_main)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_test_case(test_arena  arena              util/test_arena.cpp)
add_test_case(test_slice  gtest_main          include/test_slice.cpp)
target_include_directories(test_slice PRIVATE ${CMAKE_SOURCE_DIR})
add_test_case(test_coding coding              util/test_coding.cpp)
add_test_case(test_hash   hash                util/test_hash.cpp)
add_test_case(test_crc32c crc32c              util/test_crc32c.cpp)
add_test_case(test_bloom  bloom               util/test_bloom.cpp)
