#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "../utf8.hpp"
#include "../word_piece.hpp"
#include "naive.hpp"

int main(int argc, char *argv[]) {
    auto ts_start = word_piece::detail::currentTs();
    if (argc != 4) {
        throw std::runtime_error(
            "Usage: ./runner <mode> <text_filepath> <vocab_filepath>. Modes: naive, real.");
    }

    std::string mode = argv[1];
    std::string text_filepath = argv[2];
    std::string vocab_filepath = argv[3];

    // TODO: mmap read or read with O_DIRECT
    std::string text;
    {
        std::ifstream fin(text_filepath);
        text = std::string(std::istreambuf_iterator<char>(fin), std::istreambuf_iterator<char>());
    }

    std::vector<std::string> vocab;
    {
        std::ifstream fin(vocab_filepath);
        std::string word;
        while (std::getline(fin, word)) {
            vocab.push_back(std::move(word));
        }
    }

    std::vector<int> ids;

    if (mode == "naive") {
        ids = naiveTokenization(text, vocab);
    } else if (mode == "real") {
        ids = word_piece::wordPiece(text, vocab);
    } else {
        throw std::runtime_error("Unknown mode");
    }

    std::cout << ids.size() << " " << text.size();
    auto ts_finish = word_piece::detail::currentTs();
    std::cout << "Finished in " << ts_finish - ts_start << " ms" << std::endl;
}
