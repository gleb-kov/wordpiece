// Copyright (c) 2023 Gleb Koveshnikov

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "src/utils.hpp"
#include "src/word_piece.hpp"

int main(int argc, char *argv[]) {
    if (argc != 4 && argc != 5) {
        throw std::runtime_error(
            "Usage: ./runner <mode> <text_filepath> <vocab_filepath> [n_threads]. Modes: fast, linear.");
    }

    std::string mode = argv[1];
    std::string text_filepath = argv[2];
    std::string vocab_filepath = argv[3];
    size_t n_threads = argc == 5 ? std::stoull(argv[4]) : 0;

    // init thread pool with given number of threads
    [[maybe_unused]] auto &thread_pool = utils::globalThreadPool(n_threads);

    std::vector<int> ids;

    if (mode == "fast") {
        ids = word_piece::fastWordPiece(text_filepath, vocab_filepath);
    } else if (mode == "linear") {
        ids = word_piece::linearWordPiece(text_filepath, vocab_filepath);
    } else {
        throw std::runtime_error("Unknown mode");
    }

    std::cout << "Total ids " << ids.size() << std::endl;
}
