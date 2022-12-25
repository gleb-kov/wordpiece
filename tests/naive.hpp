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

inline std::vector<int> naiveTokenization(const std::string_view s,
                                          const std::vector<std::string> &vocab) {
    std::unordered_map<std::string_view, int> word_to_id;
    size_t max_len = 0;
    for (int i = 0; i < static_cast<int64_t>(vocab.size()); i++) {
        assert(!vocab[i].empty());
        assert(word_to_id.count(vocab[i]) == 0);
        word_to_id[vocab[i]] = i;
        max_len = std::max(max_len, vocab[i].size());
    }
    max_len = std::min(max_len, s.size());

    size_t start = 0;
    size_t len = std::min(s.size(), max_len);

    std::vector<int> tokens;
    tokens.reserve(s.size() / max_len + 1);
    while (len > 0) {
        std::string_view test{s.data() + start, len};

        auto it = word_to_id.find(test);
        if (it != word_to_id.end()) {
            tokens.push_back(it->second);
            start += len;
            len = std::min(max_len, s.size() - start);
        } else {
            --len;
        }
    }

    if (start == s.size()) {
        return tokens;
    }

    return {};
}

#endif // NAIVE_WORD_PIECE_H
