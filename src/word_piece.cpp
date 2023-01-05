// Copyright (c) 2023 Gleb Koveshnikov

#include "word_piece.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "lcp.hpp"
#include "third_party/libsais.h"
#include "third_party/utf8.hpp"
#include "utils.hpp"

static std::vector<int> wordPieceImpl(const std::vector<uint32_t> &text,
                                      const std::vector<std::vector<uint32_t>> &vocab,
                                      int unk_token_id) {
    using Count = int32_t;
    static_assert(std::is_same_v<Count, int32_t>, "64-bit unsupported"); // TODO

    size_t total_length = text.size() + 1;
    size_t longest_word_vocab = 1;
    for (const auto& word : vocab) {
        total_length += word.size() + 1;
        longest_word_vocab = std::max(longest_word_vocab, word.size());
    }

    Count *S = new Count[total_length];
    uint32_t alphabet_size = 1;

    {
        size_t pos = 0;
        for (uint32_t c : text) {
            S[pos++] = static_cast<Count>(c);
            alphabet_size = std::max(alphabet_size, c);
        }
        S[pos++] = 1;
        for (const std::vector<uint32_t> &word : vocab) {
            for (uint32_t c : word) {
                S[pos++] = static_cast<Count>(c);
                alphabet_size = std::max(alphabet_size, c);
            }
            S[pos++] = 1;
        }
    }

    if (total_length > 2'000'000'000ull || alphabet_size > 2'000'000'000) {
        throw std::runtime_error("64bit not implemented");
    }

    size_t fs = 0;
    if (total_length > 1'000'000 && total_length > alphabet_size && alphabet_size < 100'000'000) {
        fs = 6 * alphabet_size;
        if (fs > total_length) {
            fs = 4 * alphabet_size;
        }
        if (fs > total_length) {
            fs = alphabet_size;
        }
    }
    Count *suf = new Count[total_length + fs];
    Count saca_rc = 0;

#if defined(_OPENMP)
#pragma message "libsais compiled with openmp"
    Count threads_count = total_length > 10'000'000 ? 0 : 1;
    saca_rc = libsais_int_omp(S, suf, static_cast<Count>(total_length), static_cast<Count>(alphabet_size + 1), static_cast<Count>(fs), threads_count);
#else
#pragma message "libsais compiled without openmp"
    saca_rc = libsais_int(S, suf, static_cast<Count>(total_length), static_cast<Count>(alphabet_size + 1), static_cast<Count>(fs));
#endif

    if (saca_rc != 0) {
        throw std::runtime_error("SACA return code: " + std::to_string(saca_rc));
    }

    // NOTE: libsais has PLCP and LCP functions, but they will take longer.
    std::vector<Count> suf_array_index(total_length);
    for (size_t i = 0; i < total_length; i++) {
        suf_array_index[static_cast<size_t>(suf[i])] = static_cast<Count>(i);
    }

    std::vector<Count> lcp = lcp::calcLcp<Count>(S, suf, suf_array_index);
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
            detail::globalThreadPool().submit([&L, &get_closest] { L = get_closest(false); });
            detail::globalThreadPool().submit([&R, &get_closest] { R = get_closest(true); });
            detail::globalThreadPool().waitCompletion();
        }
    }

    const auto match_word_piece_suffix
        = [total_length, unk_token_id, &text, &vocab, &L, &R, &suf_array_index](
              size_t begin,
              size_t end) {
              const size_t vocab_length = total_length - text.size();
              std::vector<int> token_ids;
              token_ids.reserve((end - begin) * vocab.size() / vocab_length);

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
              return token_ids;
          };

    std::vector<int> token_ids;
    {
        static constexpr size_t kWorkBatch = 1'000'000;

        if (text.size() < 2 * kWorkBatch) {
            token_ids = match_word_piece_suffix(0, text.size());
        } else {
            const size_t thread_count
                = std::min(detail::globalThreadPool().maxThreads(), text.size() / kWorkBatch);
            const size_t work_batch = text.size() / thread_count + 1;
            std::vector<std::vector<int>> per_thread_token_ids(thread_count);
            size_t work_start = 0;
            for (size_t thread_id = 0; thread_id < thread_count && work_start < text.size();
                 thread_id++) {
                size_t work_end = std::min(text.size(), work_start + work_batch);
                while (work_end < text.size() && !vkcom::is_space(text[work_end])) {
                    ++work_end;
                }
                detail::globalThreadPool().submit([thread_id,
                                                   work_start,
                                                   work_end,
                                                   &match_word_piece_suffix,
                                                   &per_thread_token_ids] {
                    per_thread_token_ids[thread_id] = match_word_piece_suffix(work_start, work_end);
                });
                work_start = work_end;
            }
            detail::globalThreadPool().waitCompletion();

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

namespace word_piece {

std::vector<int>
wordPiece(const std::string &text, const std::vector<std::string> &vocab, int unk_token_id) {
    if (text.empty()) {
        return {};
    }
    const std::vector<uint32_t> text_utf8 = detail::parseText(text, detail::globalThreadPool());
    const std::vector<std::vector<uint32_t>> vocab_utf8 = detail::parseVocab(vocab);

    return ::wordPieceImpl(text_utf8, vocab_utf8, unk_token_id);
}

std::vector<int>
wordPiece(const std::string &text_filepath, const std::string &vocab_filepath, int unk_token_id) {
    const std::vector<uint32_t> text_utf8 = detail::readTextFromFile(text_filepath, detail::globalThreadPool());
    if (text_utf8.empty()) {
        return {};
    }
    const std::vector<std::vector<uint32_t>> vocab_utf8 = detail::readVocabFromFile(vocab_filepath);

    return ::wordPieceImpl(text_utf8, vocab_utf8, unk_token_id);
}

} // namespace word_piece
