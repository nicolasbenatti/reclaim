cmake_minimum_required(VERSION 3.14)
project(reclaim C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Optional sanitizer support: cmake -DSANITIZE=asan|tsan
if(SANITIZE STREQUAL "asan")
    add_compile_options(-O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
elseif(SANITIZE STREQUAL "tsan")
    add_compile_options(-O1 -fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
else()
    add_compile_options(-O2)
endif()

add_compile_options(-Wall -D_GNU_SOURCE)

find_package(Threads REQUIRED)

# Collect all allocator sources (excluding main.c)
file(GLOB RECLAIM_SRCS "src/*.c")

add_library(reclaim_lib STATIC ${RECLAIM_SRCS})
target_include_directories(reclaim_lib PUBLIC include PRIVATE src)
target_link_libraries(reclaim_lib PUBLIC Threads::Threads)

# Main executable
add_executable(reclaim main.c)
target_include_directories(reclaim PRIVATE include src)
target_link_libraries(reclaim PRIVATE reclaim_lib)

enable_testing()
add_subdirectory(tests)
