cmake_minimum_required(VERSION 3.0)
project(word_piece C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -DNDEBUG -g0")

if (NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")

    add_compile_options(
        -Wall
        -Werror
        -Wextra
        -Wold-style-cast
        -Wpedantic

        -fdiagnostics-color=always
    )
else ()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=thin")

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

if (CMAKE_USE_SANITIZERS)
    message(WARNING "Compiling with sanitizers")
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined)
    add_link_options(-fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined)
else ()
    add_link_options()
endif ()

# clang-tidy -p build tests/tests.cpp tests/runner.cpp tests/naive.hpp src/word_piece.hpp src/word_piece.cpp src/utils.hpp src/utils.cpp src/third_party/saca_dc3.hpp src/third_party/thread_pool.hpp src/third_party/utf8.hpp src/third_party/utf8.cpp -- --std=c++17
# clang-format -i --style=file tests/tests.cpp tests/runner.cpp tests/naive.hpp src/word_piece.hpp src/word_piece.cpp src/utils.hpp src/utils.cpp

add_subdirectory(src)
add_subdirectory(tests)
