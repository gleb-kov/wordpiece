cmake_minimum_required(VERSION 3.0)
project(wordpiece CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -DNDEBUG -g1 -flto=thin")
if (NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
endif ()

if (CMAKE_USE_SANITIZERS)
    message(WARNING "Compiling with sanitizers")
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined -fsanitize=thread)
    add_link_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined -fsanitize=thread)
else ()
    add_link_options()
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(
        -Wall
        -Werror
        -Wextra
        -Wold-style-cast
        -Wpedantic

        -fdiagnostics-color=always
    )
else ()
    add_compile_options(
        -Weverything
        -Wno-c++98-compat-pedantic
        -Wno-ctad-maybe-unsupported
        -Wno-exit-time-destructors
        -Wno-missing-prototypes
        -Wno-padded

        -fcolor-diagnostics
    )
endif ()

# clang-tidy -p tests/build tests/tests.cpp tests/runner.cpp word_piece.hpp -- --std=c++17
# clang-format -i --style=file tests/tests.cpp tests/runner.cpp word_piece.hpp
add_executable(runner runner.cpp)
add_executable(tests tests.cpp)
