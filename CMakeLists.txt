cmake_minimum_required(VERSION 3.16)
project(cache_adaptive_btree LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CABTREE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
set(CABTREE_SANITIZER "none" CACHE STRING "Sanitizer mode: none, address-undefined, or thread")
set_property(CACHE CABTREE_SANITIZER PROPERTY STRINGS none address-undefined thread)

add_library(cabtree INTERFACE)
target_include_directories(cabtree INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cabtree INTERFACE -Wall -Wextra -Wpedantic)
  if(CABTREE_WARNINGS_AS_ERRORS)
    target_compile_options(cabtree INTERFACE -Werror)
  endif()
else()
  message(FATAL_ERROR "Codespaces validation supports GCC or Clang only")
endif()

if(CABTREE_SANITIZER STREQUAL "address-undefined")
  target_compile_options(cabtree INTERFACE -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined)
  target_link_options(cabtree INTERFACE -fsanitize=address,undefined)
  target_compile_definitions(cabtree INTERFACE CABTREE_SANITIZER_MODE="asan-ubsan")
elseif(CABTREE_SANITIZER STREQUAL "thread")
  target_compile_options(cabtree INTERFACE -O1 -g -fno-omit-frame-pointer -fsanitize=thread)
  target_link_options(cabtree INTERFACE -fsanitize=thread)
  target_compile_definitions(cabtree INTERFACE CABTREE_SANITIZER_MODE="tsan")
elseif(CABTREE_SANITIZER STREQUAL "none")
  target_compile_definitions(cabtree INTERFACE CABTREE_SANITIZER_MODE="none")
else()
  message(FATAL_ERROR "Unknown CABTREE_SANITIZER=${CABTREE_SANITIZER}")
endif()

function(cabtree_add_test target source)
  add_executable(${target} "tests/${source}")
  target_link_libraries(${target} PRIVATE cabtree Threads::Threads)
  add_test(NAME ${target} COMMAND ${target})
endfunction()

cabtree_add_test(test_btree test_btree.cpp)
cabtree_add_test(test_monitor test_monitor.cpp)
cabtree_add_test(test_predictor test_predictor.cpp)
cabtree_add_test(test_adaptive test_adaptive.cpp)
cabtree_add_test(test_adaptive_regression test_adaptive_regression.cpp)
cabtree_add_test(test_adaptive_aco test_adaptive_aco.cpp)
cabtree_add_test(test_segmented_adaptive test_segmented_adaptive.cpp)
cabtree_add_test(test_v2 test_v2.cpp)
cabtree_add_test(test_workload_identity test_workload_identity.cpp)
cabtree_add_test(simple_test simple_test.cpp)

add_executable(test_btree_concurrent_correctness tests/test_btree_concurrent_correctness.cpp)
target_link_libraries(test_btree_concurrent_correctness PRIVATE cabtree Threads::Threads)
add_test(NAME test_btree_concurrent_correctness
         COMMAND test_btree_concurrent_correctness
                 "${CMAKE_BINARY_DIR}/concurrent_correctness.csv")

