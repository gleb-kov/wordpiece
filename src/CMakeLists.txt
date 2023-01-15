add_subdirectory(third_party)

add_library(word_piece STATIC
            fast.cpp
            linear.cpp
            utils.cpp)

target_link_libraries(word_piece PUBLIC third_party)
