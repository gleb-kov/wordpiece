add_subdirectory(third_party)

add_library(word_piece STATIC
            utils.cpp
            word_piece.cpp)

target_link_libraries(word_piece PUBLIC third_party)
