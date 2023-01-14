add_executable(runner runner.cpp)
add_executable(tests tests.cpp)

target_link_libraries(runner word_piece third_party)
target_link_libraries(tests word_piece third_party)

target_include_directories(runner PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(tests PRIVATE ${PROJECT_SOURCE_DIR})
