// Copyright (c) 2023 Gleb Koveshnikov

#pragma once

#include <algorithm>
#include <cstring>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "third_party/saca_dc3.hpp"
#include "third_party/thread_pool.hpp"
#include "third_party/utf8.hpp"
#include "utils.hpp"

namespace word_piece {

inline detail::ThreadPool &globalThreadPool() {
    static detail::ThreadPool thread_pool;
    return thread_pool;
}

template <typename Count>
inline void calcLcpImpl(const uint32_t *str,
                        const Count *suf_a,
                        const std::vector<Count> &suf_array_index,
                        std::vector<Count> &lcp,
                        size_t begin,
                        size_t end) {
    size_t prefix_len = 0;
    for (size_t i = begin; i < end; i++) {
        const size_t sa_index = static_cast<size_t>(suf_array_index[i]);
        if (sa_index + 1 != suf_array_index.size()) {
            const size_t suf_index = static_cast<size_t>(suf_a[sa_index + 1]);

            while (std::max(i, suf_index) + prefix_len < suf_array_index.size()
                   && str[i + prefix_len] == str[suf_index + prefix_len]) {
                prefix_len++;
            }
            lcp[sa_index] = static_cast<Count>(prefix_len);
            if (prefix_len > 0) {
                prefix_len--;
            }
        }
    }
}

template <typename Count>
inline std::vector<Count>
calcLcp(const uint32_t *str, const Count *suf_a, const std::vector<Count> &suf_array_index) {
    static constexpr size_t kWorkBatch = 1'000'000;
    const size_t total_length = suf_array_index.size();

    std::vector<Count> lcp(total_length - 1);

    if (total_length < 2 * kWorkBatch) {
        calcLcpImpl(str, suf_a, suf_array_index, lcp, 0, total_length);
    } else {
        const size_t thread_count
            = std::min(globalThreadPool().maxThreads(), total_length / kWorkBatch);
        const size_t work_batch = total_length / thread_count + 1;
        size_t work_start = 0;
        for (size_t i = 0; i < thread_count; i++) {
            size_t work_end = std::min(total_length, work_start + work_batch);
            globalThreadPool().submit(
                [str, suf_a, work_start, work_end, &suf_array_index, &lcp] {
                    calcLcpImpl(str, suf_a, suf_array_index, lcp, work_start, work_end);
                });
            work_start = work_end;
        }
    }

    globalThreadPool().waitCompletion();

    return lcp;
}

template <typename Count>
inline std::vector<int> wordPieceImpl(const std::vector<uint32_t> &text,
                                      const std::vector<std::vector<uint32_t>> &vocab,
                                      int unk_token_id) {
    size_t total_length = text.size() + 1;
    size_t longest_word_vocab = 1;
    for (const auto& word : vocab) {
        total_length += word.size() + 1;
        longest_word_vocab = std::max(longest_word_vocab, word.size());
    }

    uint32_t *S = new uint32_t[total_length + 3];
    uint32_t alphabet_size = 1;

    {
        size_t pos = 0;
        for (uint32_t c : text) {
            S[pos++] = c;
            alphabet_size = std::max(alphabet_size, c);
        }
        S[pos++] = 1;
        for (const std::vector<uint32_t> &word : vocab) {
            for (uint32_t c : word) {
                S[pos++] = c;
                alphabet_size = std::max(alphabet_size, c);
            }
            S[pos++] = 1;
        }
    }

    S[total_length] = S[total_length + 1] = S[total_length + 2] = 0;
    Count *suf = new Count[total_length + 3];
    // auto t1 = detail::currentTs();
    saca_dc3::suffixArray<uint32_t, Count>(S, suf, total_length, alphabet_size);
    // auto t2 = detail::currentTs();
    // std::cout << "sa " << t2 - t1 << '\n';

    std::vector<Count> suf_array_index(total_length);
    for (size_t i = 0; i < total_length; i++) {
        suf_array_index[static_cast<size_t>(suf[i])] = static_cast<Count>(i);
    }

    std::vector<Count> lcp = calcLcp<Count>(S, suf, suf_array_index);
    delete[] S;
    delete[] suf;

    static constexpr int kNoMatchedSuffix = -1;
    std::vector<int> who(total_length, kNoMatchedSuffix);

    size_t vocab_start_pos = text.size() + 1;
    for (size_t i = 0; i < vocab.size(); i++) {
        who[static_cast<size_t>(suf_array_index[vocab_start_pos])] = static_cast<int>(i);
        vocab_start_pos += vocab[i].size() + 1;
    }
    const auto get_closest = [longest_word_vocab, total_length, &lcp, &who, &vocab](bool reverse) {
        std::vector<int> result(total_length, kNoMatchedSuffix);
        // (i, |i|); i is index in ts
        std::vector<std::pair<int, Count>> st;
        st.reserve(longest_word_vocab);
        for (size_t i = 0; i < total_length; i++) {
            if (i > 0) {
                const size_t index = reverse ? total_length - i - 1 : i - 1;
                while (!st.empty() && st.back().second > lcp[index]) {
                    st.pop_back();
                }
            }

            const size_t index = reverse ? total_length - 1 - i : i;
            if (who[index] != kNoMatchedSuffix) {
                Count len = static_cast<Count>(vocab[static_cast<size_t>(who[index])].size());
                st.emplace_back(who[index], len);
            }
            if (!st.empty()) {
                result[i] = st.back().first;
            }
        }
        return result;
    };

    std::vector<int> L;
    std::vector<int> R;
    {
        static constexpr size_t kWorkBatch = 1'000'000;
        if (total_length < kWorkBatch) {
            L = get_closest(false);
            R = get_closest(true);
        } else {
            globalThreadPool().submit([&L, &get_closest] { L = get_closest(false); });
            globalThreadPool().submit([&R, &get_closest] { R = get_closest(true); });
            globalThreadPool().waitCompletion();
        }
    }

    const auto match_word_piece_suffix
        = [total_length, unk_token_id, &text, &vocab, &L, &R, &suf_array_index](
              size_t begin,
              size_t end,
              std::vector<int> &token_ids) {
              size_t match_index = begin;
              while (match_index != end && vkcom::is_space(text[match_index])) {
                  ++match_index;
              }

              while (match_index < end) {
                  size_t id = static_cast<size_t>(suf_array_index[match_index]);
                  int x = L[id];
                  int y = R[total_length - 1 - id];

                  if (x != kNoMatchedSuffix || y != kNoMatchedSuffix) {
                      int token_id;
                      if (x != kNoMatchedSuffix && y != kNoMatchedSuffix) {
                          token_id = vocab[static_cast<size_t>(x)].size()
                                           > vocab[static_cast<size_t>(y)].size()
                                       ? x
                                       : y;
                      } else {
                          token_id = std::max(x, y);
                      }
                      token_ids.push_back(token_id);
                      match_index += vocab[static_cast<size_t>(token_id)].size();
                  } else {
                      token_ids.push_back(unk_token_id);
                      while (match_index != end && !vkcom::is_space(text[match_index])) {
                          ++match_index;
                      }
                  }
                  while (match_index != end && vkcom::is_space(text[match_index])) {
                      ++match_index;
                  }
              }
          };

    std::vector<int> token_ids;
    {
        static constexpr size_t kWorkBatch = 1'000'000;
        const size_t vocab_length = total_length - text.size();

        if (text.size() < 2 * kWorkBatch) {
            token_ids.reserve(text.size() * vocab.size() / vocab_length);
            match_word_piece_suffix(0, text.size(), token_ids);
        } else {
            const size_t thread_count
                = std::min(globalThreadPool().maxThreads(), text.size() / kWorkBatch);
            const size_t work_batch = text.size() / thread_count + 1;
            std::vector<std::vector<int>> per_thread_token_ids(thread_count);
            size_t work_start = 0;
            for (size_t thread_id = 0; thread_id < thread_count && work_start < text.size();
                 thread_id++) {
                size_t work_end = std::min(text.size(), work_start + work_batch);
                while (work_end < text.size() && !vkcom::is_space(text[work_end])) {
                    ++work_end;
                }
                per_thread_token_ids[thread_id].reserve((work_end - work_start) * vocab.size()
                                                        / vocab_length);
                globalThreadPool().submit([thread_id,
                                           work_start,
                                           work_end,
                                           &match_word_piece_suffix,
                                           &per_thread_token_ids] {
                    match_word_piece_suffix(work_start, work_end, per_thread_token_ids[thread_id]);
                });
                work_start = work_end;
            }
            globalThreadPool().waitCompletion();

            size_t token_count = 0;
            for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
                token_count += per_thread_token_ids[thread_id].size();
            }
            token_ids.resize(token_count);
            work_start = 0;
            for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
                std::vector<int> &segment = per_thread_token_ids[thread_id];
                if (!segment.empty()) {
                    std::memcpy(token_ids.data() + work_start,
                                segment.data(),
                                segment.size() * sizeof(int));
                    work_start += segment.size();
                }
            }
        }
    }

    return token_ids;
}

inline std::vector<int>
wordPiece(const std::vector<uint32_t> &text, const std::vector<std::vector<uint32_t>> &vocab, int unk_token_id) {
    // gives 6% speed boost due to cache and alloc optimizations.
    if (text.size() < 2'000'000'000) {
        return wordPieceImpl<uint32_t>(text, vocab, unk_token_id);
    } else {
        return wordPieceImpl<size_t>(text, vocab, unk_token_id);
    }
}

inline std::vector<int>
wordPiece(const std::string &text, const std::vector<std::string> &vocab, int unk_token_id = -1) {
    if (text.empty()) {
        return {};
    }
    const std::vector<uint32_t> text_utf8 = detail::parseText(text, globalThreadPool());
    const std::vector<std::vector<uint32_t>> vocab_utf8 = detail::parseVocab(vocab);

    return wordPiece(text_utf8, vocab_utf8, unk_token_id);
}

inline std::vector<int>
wordPiece(const std::string &text_filepath, const std::string &vocab_filepath, int unk_token_id = -1) {
    const std::vector<uint32_t> text_utf8 = detail::readTextFromFile(text_filepath, globalThreadPool());
    if (text_utf8.empty()) {
        return {};
    }
    const std::vector<std::vector<uint32_t>> vocab_utf8 = detail::readVocabFromFile(vocab_filepath);

    return wordPiece(text_utf8, vocab_utf8, unk_token_id);
}

} // namespace word_piece
