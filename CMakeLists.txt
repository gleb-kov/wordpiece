cmake_minimum_required(VERSION 3.20)
project(wordpiece C CXX)

set(CONAN_SYSTEM_INCLUDES "On")
set(CONAN_PROFILE default)

if (NOT DEFINED ENV{CXX})
    set(ENV{CXX} ${CMAKE_CXX_COMPILER})
endif ()

if (NOT DEFINED ENV{CC})
    set(ENV{CC} ${CMAKE_C_COMPILER})
endif ()

include(conan/conan.cmake)
conan_cmake_autodetect(settings)

conan_cmake_run(
        CONANFILE ${CMAKE_CURRENT_SOURCE_DIR}/conan/conanfile.txt
        BASIC_SETUP
        BUILD missing
        SETTINGS ${settings}
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

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
    string(REPLACE ${CONAN_LIBS_TCMALLOC} "" CONAN_LIBS "${CONAN_LIBS}")
else ()
    add_link_options()
endif ()

include_directories(src/)
add_executable(wordpiece main.cpp)
target_link_libraries(wordpiece ${CONAN_LIBS})

add_executable(benchmark_micro benchmark_micro.cpp)
target_link_libraries(benchmar_micro ${CONAN_LIBS})

file(GLOB_RECURSE TEST_SOURCES tests/*.cpp tests/*.hpp)
if (TEST_SOURCES)
    enable_testing()
    add_executable(tests ${TEST_SOURCES})
    target_compile_definitions(tests PUBLIC TESTING)
    target_compile_options(tests PUBLIC -Wno-unreachable-code)
    target_link_libraries(tests ${CONAN_LIBS})
endif ()
