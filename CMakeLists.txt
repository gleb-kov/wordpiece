cmake_minimum_required(VERSION 3.9)
project(word_piece C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -DNDEBUG -g0")

if (NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")

    add_compile_options(
        -Wall
        -Werror
        -Wextra
        -Wpedantic

        -fdiagnostics-color=always
    )
else ()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=thin")

    add_compile_options(
        -Weverything
        -Wno-c++98-compat-pedantic
        -Wno-missing-prototypes
        -Wno-padded

        -fcolor-diagnostics
    )
endif ()

if (CMAKE_USE_SANITIZERS)
    message(WARNING "Compiling with sanitizers")
    add_compile_options(-fno-omit-frame-pointer -fsanitize=address,undefined,leak)
    add_link_options(-fno-omit-frame-pointer -fsanitize=address,undefined,leak)
else ()
    add_link_options()
endif ()

add_subdirectory(src)
add_subdirectory(tests)
