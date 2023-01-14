// Copyright (c) 2023 Gleb Koveshnikov

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "src/utils.hpp"
#include "src/word_piece.hpp"

int main(int argc, char *argv[]) {
    auto ts_start = detail::currentTs();
    if (argc != 4) {
        throw std::runtime_error(
            "Usage: ./runner <mode> <text_filepath> <vocab_filepath>. Modes: fast, linear.");
    }

    std::string mode = argv[1];
    std::string text_filepath = argv[2];
    std::string vocab_filepath = argv[3];

    std::vector<int> ids;

    if (mode == "fast") {
        ids = word_piece::fastWordPiece(text_filepath, vocab_filepath);
    } else if (mode == "linear") {
        ids = word_piece::linearWordPiece(text_filepath, vocab_filepath);
    } else {
        throw std::runtime_error("Unknown mode");
    }

    auto ts_finish = detail::currentTs();
    std::cout << "Finished in " << ts_finish - ts_start << " ms; ids " << ids.size() << std::endl;
}
