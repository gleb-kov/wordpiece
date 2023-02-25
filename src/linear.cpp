// Copyright (c) 2023 Gleb Koveshnikov

#include "word_piece.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <boost/iostreams/device/mapped_file.hpp>

#include "third_party/libsais.h"
#include "third_party/utf8.hpp"
#include "utils.hpp"

template <typename Count>
static void calcLcpImpl(const Count *str,
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
static std::vector<Count>
calcLcp(const Count *str, const Count *suf_a, const std::vector<Count> &suf_array_index) {
  static constexpr size_t kWorkBatch = 1'000'000;
  const size_t total_length = suf_array_index.size();

  std::vector<Count> lcp(total_length - 1);

  if (total_length < 2 * kWorkBatch) {
    calcLcpImpl(str, suf_a, suf_array_index, lcp, 0, total_length);
  } else {
    const size_t thread_count
     = std::min(utils::globalThreadPool().maxThreads(), total_length / kWorkBatch);
    const size_t work_batch = total_length / thread_count + 1;
    size_t work_start = 0;
    for (size_t i = 0; i < thread_count; i++) {
      size_t work_end = std::min(total_length, work_start + work_batch);
      utils::globalThreadPool().submit([str, suf_a, work_start, work_end, &suf_array_index, &lcp] {
        calcLcpImpl(str, suf_a, suf_array_index, lcp, work_start, work_end);
      });
      work_start = work_end;
    }
  }

  utils::globalThreadPool().waitCompletion();

  return lcp;
}

static std::vector<int> linearWordPieceImpl(const std::vector<uint32_t> &text,
                                            const utils::WordPieceVocabulary &vocab) {
  using Count = int32_t;
  static_assert(std::is_same_v<Count, int32_t>, "64-bit unsupported"); // TODO

  size_t total_length = text.size() + 1;
  size_t longest_word_vocab = 1;
  for (const auto &token : vocab.tokens) {
    total_length += token.word.size() + 1;
    longest_word_vocab = std::max(longest_word_vocab, token.word.size());
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
    for (const utils::WordPieceToken &token : vocab.tokens) {
      for (uint32_t c : token.word) {
        S[pos++] = static_cast<Count>(c);
        alphabet_size = std::max(alphabet_size, c);
      }
      S[pos++] = 1;
    }
  }

  ++alphabet_size; // [0; max symbol]
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
  saca_rc = libsais_int_omp(S,
                            suf,
                            static_cast<Count>(total_length),
                            static_cast<Count>(alphabet_size),
                            static_cast<Count>(fs),
                            threads_count);
#else
#pragma message "libsais compiled without openmp"
  saca_rc = libsais_int(S,
                        suf,
                        static_cast<Count>(total_length),
                        static_cast<Count>(alphabet_size),
                        static_cast<Count>(fs));
#endif

  if (saca_rc != 0) {
    throw std::runtime_error("SACA return code: " + std::to_string(saca_rc));
  }

  // NOTE: libsais has PLCP and LCP functions, but they will take longer.
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
  for (size_t i = 0; i < vocab.tokens.size(); i++) {
    who[static_cast<size_t>(suf_array_index[vocab_start_pos])] = static_cast<int>(i);
    vocab_start_pos += vocab.tokens[i].word.size() + 1;
  }
  const auto get_closest
   = [longest_word_vocab, total_length, &lcp, &who, &vocab](bool right_side,
                                                            bool is_prefix_predicate) {
       std::vector<int> result(total_length, kNoMatchedSuffix);
       // (i, |i|); i is index in ts
       std::vector<std::pair<int, Count>> st;
       st.reserve(longest_word_vocab);
       for (size_t i = 0; i < total_length; i++) {
         if (i > 0) {
           const size_t index = right_side ? total_length - i - 1 : i - 1;
           while (!st.empty() && st.back().second > lcp[index]) {
             st.pop_back();
           }
         }

         const size_t index = right_side ? total_length - 1 - i : i;
         if (who[index] != kNoMatchedSuffix) {
           const auto &token = vocab.tokens[static_cast<size_t>(who[index])];
           if (token.is_prefix == is_prefix_predicate && !token.is_malformed && !token.is_special) {
             Count len = static_cast<Count>(token.word.size());
             st.emplace_back(who[index], len);
           }
         }
         if (!st.empty()) {
           result[i] = st.back().first;
         }
       }
       return result;
     };

  std::vector<int> best_left_prefix;
  std::vector<int> best_right_prefix;
  std::vector<int> best_left_suffix;
  std::vector<int> best_right_suffix;
  {
    static constexpr size_t kWorkBatch = 1'000'000;
    if (total_length < kWorkBatch) {
      best_left_prefix = get_closest(false, true);
      best_right_prefix = get_closest(true, true);
      best_left_suffix = get_closest(false, false);
      best_right_suffix = get_closest(true, false);
    } else {
      utils::globalThreadPool().submit(
       [&best_left_prefix, &get_closest] { best_left_prefix = get_closest(false, true); });
      utils::globalThreadPool().submit(
       [&best_right_prefix, &get_closest] { best_right_prefix = get_closest(true, true); });
      utils::globalThreadPool().submit(
       [&best_left_suffix, &get_closest] { best_left_suffix = get_closest(false, false); });
      utils::globalThreadPool().submit(
       [&best_right_suffix, &get_closest] { best_right_suffix = get_closest(true, false); });
      utils::globalThreadPool().waitCompletion();
    }
  }

  const auto is_word_prefix = [&text](size_t index) {
    // std::cout << index << ' ' << vkcom::is_spacing_char(text[index]) << std::endl;
    return index == 0 || vkcom::is_spacing_char(text[index])
        || vkcom::is_spacing_char(text[index - 1]);
  };

  const auto match_word_piece
   = [&, unk_token_id = vocab.unk_token_id](size_t match_index, size_t end) {
       const size_t vocab_length = total_length - text.size();
       std::vector<int> token_ids;
       token_ids.reserve((end - match_index) * vocab.tokens.size() / vocab_length);

       while (match_index != end && vkcom::is_space(text[match_index])) {
         ++match_index;
       }

       size_t tokens_since_prefix = 0;

       while (match_index < end) {
         const size_t left_sa_id = static_cast<size_t>(suf_array_index[match_index]);
         const size_t right_sa_id = total_length - 1 - left_sa_id;
         const bool prefix = is_word_prefix(match_index);
         const int x = prefix ? best_left_prefix[left_sa_id] : best_left_suffix[left_sa_id];
         const int y = prefix ? best_right_prefix[right_sa_id] : best_right_suffix[right_sa_id];

         if (x != kNoMatchedSuffix || y != kNoMatchedSuffix) {
           int token_id;
           if (x != kNoMatchedSuffix && y != kNoMatchedSuffix) {
             token_id = vocab.tokens[static_cast<size_t>(x)].word.size()
                        > vocab.tokens[static_cast<size_t>(y)].word.size()
                       ? x
                       : y;
           } else {
             token_id = std::max(x, y);
           }
           ++tokens_since_prefix;
           token_ids.push_back(token_id);
           match_index += vocab.tokens[static_cast<size_t>(token_id)].word.size();

           if (match_index != end && is_word_prefix(match_index)) {
             tokens_since_prefix = 0;
           }
         } else {
           while (tokens_since_prefix > 0) {
             token_ids.pop_back();
             --tokens_since_prefix;
           }
           token_ids.push_back(unk_token_id);
           ++match_index;
           while (match_index != end && !is_word_prefix(match_index)) {
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
      token_ids = match_word_piece(0, text.size());
    } else {
      const size_t thread_count
       = std::min(utils::globalThreadPool().maxThreads(), text.size() / kWorkBatch);
      const size_t work_batch = text.size() / thread_count + 1;
      std::vector<std::vector<int>> per_thread_token_ids(thread_count);
      size_t work_start = 0;
      for (size_t thread_id = 0; thread_id < thread_count && work_start < text.size();
           thread_id++) {
        size_t work_end = std::min(text.size(), work_start + work_batch);
        while (work_end < text.size() && !vkcom::is_space(text[work_end])) {
          ++work_end;
        }
        utils::globalThreadPool().submit(
         [thread_id, work_start, work_end, &match_word_piece, &per_thread_token_ids] {
           per_thread_token_ids[thread_id] = match_word_piece(work_start, work_end);
         });
        work_start = work_end;
      }
      utils::globalThreadPool().waitCompletion();

      size_t token_count = 0;
      for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
        token_count += per_thread_token_ids[thread_id].size();
      }
      token_ids.resize(token_count);
      work_start = 0;
      for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
        std::vector<int> &segment = per_thread_token_ids[thread_id];
        if (!segment.empty()) {
          std::memcpy(token_ids.data() + work_start, segment.data(), segment.size() * sizeof(int));
          work_start += segment.size();
        }
      }
    }
  }

  return token_ids;
}

static std::vector<int>
linearWordPiece(const char *text, size_t size, const utils::WordPieceVocabulary &vocab) {
  if (size == 0) {
    return {};
  }
  const std::vector<uint32_t> text_utf8 = utils::parseText(text, size, utils::globalThreadPool());
  return linearWordPieceImpl(text_utf8, vocab);
}

namespace word_piece {

std::vector<int> linear(const std::string &text, const std::vector<std::string> &vocab) {
  const utils::WordPieceVocabulary vocab_utf8 = utils::parseVocab(vocab);
  return linearWordPiece(text.data(), text.size(), vocab_utf8);
}

std::vector<int> linear(const std::string &text_file, const std::string &vocab_file) {
  const utils::WordPieceVocabulary vocab_utf8 = utils::readVocabFromFile(vocab_file);
  boost::iostreams::mapped_file mmap(text_file, boost::iostreams::mapped_file::readonly);
  return linearWordPiece(mmap.const_data(), mmap.size(), vocab_utf8);
}

void linearExternal(const std::string &text_file,
                    const std::string &vocab_file,
                    const std::string &out_file,
                    size_t memory_limit) {
  const utils::WordPieceVocabulary vocab_utf8 = utils::readVocabFromFile(vocab_file);

  const size_t maxTextBatch = memory_limit / 20; // because of SAIS
  boost::iostreams::mapped_file mmap(text_file, boost::iostreams::mapped_file::readonly);
  const char *begin = mmap.const_data();
  size_t size = mmap.size();

  std::ofstream fout(out_file);
  while (size > 0) {
    size_t batch;
    if (size > maxTextBatch) {
      batch = maxTextBatch;
      while (batch < size
             && !vkcom::starts_with_space(begin + batch - 1, static_cast<int64_t>(size - batch))) {
        batch++;
      }
    } else {
      batch = size;
    }

    std::vector<int> ids = linearWordPiece(begin, batch, vocab_utf8);
    for (int id : ids) {
      fout << id << ' ';
    }
    begin += batch;
    size -= batch;
  }
}

} // namespace word_piece
