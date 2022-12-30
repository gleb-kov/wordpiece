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
    std::unordered_map<vkcom::VectorSegment, int> word_to_id;
    size_t max_len = 0;
    for (size_t i = 0; i < vocab.size(); i++) {
        assert(!vocab[i].empty());
        assert(word_to_id.count(vocab[i]) == 0);
        vkcom::VectorSegment segment(vocab[i]);
        word_to_id[segment] = static_cast<int>(i);
        max_len = std::max(max_len, vocab[i].size());
    }
    max_len = std::min(max_len, text.size());

    std::vector<int> token_ids;
    token_ids.reserve(text.size() / max_len + 1);

    size_t start = 0;

    while (start < text.size()) {
        const size_t len = std::min(max_len, text.size() - start);
        std::vector<uint32_t> test{text.begin() + static_cast<int64_t>(start), text.begin() + static_cast<int64_t>(start + len)};

        while (!test.empty()) {
            vkcom::VectorSegment segment(test);
            auto it = word_to_id.find(segment);
            if (it != word_to_id.end()) {
                token_ids.push_back(it->second);
                start += test.size();
                break;
            } else {
                test.pop_back();
            }
        }
        if (test.empty()) {
            token_ids.push_back(unk_token_id);
            while (start != text.size() && !vkcom::is_space(text[start])) {
                ++start;
            }
        }
        while (start != text.size() && vkcom::is_space(text[start])) {
            ++start;
        }
    }

    if (start == text.size()) {
        return token_ids;
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
