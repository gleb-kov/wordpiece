#ifndef NAIVE_WORD_PIECE_H
#define NAIVE_WORD_PIECE_H

#include <algorithm>
#include <cassert>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../utf8.hpp"

inline std::vector<int> naiveTokenization(const std::vector<uint32_t> &text,
                                          const std::vector<std::vector<uint32_t>> &vocab,
                                          int unk_token_id) {
    const int text_size = static_cast<int>(text.size());
    std::unordered_map<vkcom::VectorSegment, int> word_to_id;
    int max_len = 0;
    for (int i = 0; i < static_cast<int>(vocab.size()); i++) {
        assert(!vocab[i].empty());
        assert(word_to_id.count(vocab[i]) == 0);
        vkcom::VectorSegment segment(vocab[i]);
        word_to_id[segment] = i;
        max_len = std::max(max_len, static_cast<int>(vocab[i].size()));
    }
    max_len = std::min(max_len, text_size);

    std::vector<int> tokens;
    tokens.reserve(text_size / max_len + 1);

    int start = 0;

    while (start != text_size) {
        const int len = std::min(max_len, text_size - start);
        std::vector<uint32_t> test{text.begin() + start, text.begin() + start + len};

        while (!test.empty()) {
            vkcom::VectorSegment segment(test);
            auto it = word_to_id.find(segment);
            if (it != word_to_id.end()) {
                tokens.push_back(it->second);
                start += static_cast<int>(test.size());
                break;
            } else {
                test.pop_back();
            }
        }
        if (test.empty()) {
            tokens.push_back(unk_token_id);
            while (start != text_size && !vkcom::is_space(text[start])) {
                ++start;
            }
        }
        while (start != text_size && vkcom::is_space(text[start])) {
            ++start;
        }
    }

    if (start == static_cast<int>(text_size)) {
        return tokens;
    }

    return {};
}

inline std::vector<int> naiveTokenization(const std::string &text, const std::vector<std::string>& vocab,
                                          int unk_token_id = -1) {
    std::vector<uint32_t> text_utf8 = vkcom::decode_utf8(text);
    std::vector<std::vector<uint32_t>> vocab_utf8(vocab.size());
    for (size_t i = 0; i < vocab.size(); i++) {
        vocab_utf8[i] = vkcom::decode_utf8(vocab[i]);
    }
    return naiveTokenization(text_utf8, vocab_utf8, unk_token_id);
}

#endif // NAIVE_WORD_PIECE_H
