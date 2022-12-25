#include "naive.hpp"
#include "word_piece.hpp"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        throw std::runtime_error("Usage: ./runner <mode> <text_filepath> <vocab_filepath>. Modes: naive/real.");
    }

    std::string mode = argv[1];
    std::string text_filepath = argv[2];
    std::string vocab_filepath = argv[3];

    std::ios::sync_with_stdio(false);
    std::cin.tie(0);

    // TODO: mmap read or read with O_DIRECT
    std::string text;
    {
        std::ifstream fin(text_filepath);
        text = std::string(std::istreambuf_iterator<char>(fin), std::istreambuf_iterator<char>());
    }

    std::vector<std::string> vocab;
    {
        std::string word;
        while (std::getline(std::cin, &word)) {
            vocab.push_back(std::move(word));
        }
    }

    std::vector<int> ids;
    
    if (mode == "naive") {
        ids = naiveTokenization(text, vocab);
    } else if (mode == "real") {
        ids = word_piece::wordPiece(text, vocab);
    }
    std::cout << ids.size();
}
