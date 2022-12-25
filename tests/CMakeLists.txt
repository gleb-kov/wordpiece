cmake_minimum_required(VERSION 3.20)
project(wordpiece CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -g1 -march=native")

add_compile_options(
        -Werror
        -Wall
        -Wextra
        -Wold-style-cast
        -Wpedantic
        -Wshadow=compatible-local
        -Wcast-align=strict
        -Wlogical-op

        -fdiagnostics-color=always
)

if (CMAKE_USE_SANITIZERS)
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined)
    add_link_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined)
else ()
    add_link_options()
endif ()

add_executable(runner runner.cpp)
add_executable(tests tests.cpp)