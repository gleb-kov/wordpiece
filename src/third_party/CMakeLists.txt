add_library(third_party STATIC
            libsais.c
            utf8.cpp)

find_package(OpenMP)
if (OpenMP_CXX_FOUND AND CMAKE_USE_OPENMP)
    target_link_libraries(third_party PUBLIC OpenMP::OpenMP_C OpenMP::OpenMP_CXX)
endif ()
