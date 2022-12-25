cmake_minimum_required(VERSION 3.0)
project(wordpiece CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -g1 -march=native")

if (CMAKE_USE_SANITIZERS)
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined)
    add_link_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined)
else ()
    add_link_options()
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(
        -Wall
        -Werror
        -Wextra
        -Wpedantic
        -Wold-style-cast
        -Wshadow=compatible-local
        -Wcast-align=strict
        -Wlogical-op

        -fdiagnostics-color=always
    )
else ()
    add_compile_options(
        -Weverything
        -Wno-missing-prototypes
        -Wno-c++98-compat-pedantic
        -Wno-sign-conversion
        -Wno-ctad-maybe-unsupported

        -fcolor-diagnostics
    )
endif ()

add_executable(runner runner.cpp)
add_executable(tests tests.cpp)
