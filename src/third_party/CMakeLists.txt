add_library(third_party STATIC
            libsais.c
            utf8.cpp)

find_package(Boost 1.5 COMPONENTS iostreams REQUIRED)
include_directories(${Boost_INCLUDE_DIR})
target_link_libraries(third_party ${Boost_LIBRARIES})

find_package(OpenMP)
if (OpenMP_CXX_FOUND AND CMAKE_USE_OPENMP)
    target_link_libraries(third_party PUBLIC OpenMP::OpenMP_C OpenMP::OpenMP_CXX)
endif ()
