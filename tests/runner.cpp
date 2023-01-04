// Copyright (c) 2023 Gleb Koveshnikov

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "naive.hpp"
#include "src/utils.hpp"
#include "src/word_piece.hpp"

int main(int argc, char *argv[]) {
    auto ts_start = detail::currentTs();
    if (argc != 4) {
        throw std::runtime_error(
            "Usage: ./runner <mode> <text_filepath> <vocab_filepath>. Modes: naive, real.");
    }

    std::string mode = argv[1];
    std::string text_filepath = argv[2];
    std::string vocab_filepath = argv[3];

    std::vector<int> ids;

    if (mode == "naive") {
        ids = naive::naiveTokenization(text_filepath, vocab_filepath);
    } else if (mode == "real") {
        ids = word_piece::wordPiece(text_filepath, vocab_filepath);
    } else {
        throw std::runtime_error("Unknown mode");
    }

    auto ts_finish = detail::currentTs();
    std::cout << "Finished in " << ts_finish - ts_start << " ms; ids " << ids.size() << std::endl;
}
