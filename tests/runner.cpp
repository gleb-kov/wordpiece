// Copyright (c) 2023 Gleb Koveshnikov

#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "src/utils.hpp"
#include "src/word_piece.hpp"

int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 6) {
        throw std::runtime_error(
            "Usage: ./runner <mode> <text_filepath> <vocab_filepath> [n_threads] [out_filepath]. Modes: fast, linear.");
    }

    std::string mode = argv[1];
    std::string text_filepath = argv[2];
    std::string vocab_filepath = argv[3];
    size_t n_threads = argc == 5 ? std::stoull(argv[4]) : 0;
    std::optional<std::string> out_filepath = argc >= 6 ? std::optional(argv[5]) : std::nullopt;

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

    if (out_filepath) {
        std::ofstream fout(out_filepath.value());
        for (int id : ids) {
            fout << id << ' ';
        }
    }
}
