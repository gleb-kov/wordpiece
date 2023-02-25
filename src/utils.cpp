// Copyright (c) 2023 Gleb Koveshnikov

#include "utils.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "third_party/thread_pool.hpp"
#include "third_party/utf8.hpp"

static constexpr std::string_view kUnkTokenIdStr = "[UNK]";

namespace utils {

int64_t currentTs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
   .count();
}

ThreadPool &globalThreadPool(size_t n_threads) {
  static ThreadPool thread_pool(n_threads);
  return thread_pool;
}

void writeToFile(const std::string &file, const std::vector<int> &ids) {
  std::ofstream fout(file);
  for (int id : ids) {
    fout << id << ' ';
  }
}

std::vector<uint32_t> parseText(const char *text, size_t size, ThreadPool &thread_pool) {
  static constexpr size_t kWorkBatch = 5'000'000;

  if (size < 2 * kWorkBatch) {
    return vkcom::decode_utf8(text, text + size);
  } else {
    const size_t thread_count = std::min(thread_pool.maxThreads(), size / kWorkBatch);
    const size_t work_batch = size / thread_count + 1;
    std::vector<std::vector<uint32_t>> per_thread_text_utf8(thread_count);
    size_t work_start = 0;
    for (size_t thread_id = 0; thread_id < thread_count && work_start < size; thread_id++) {
      size_t work_end = std::min(size, work_start + work_batch);
      while (work_end < size && !vkcom::check_symbol_start(text[work_end])) {
        ++work_end;
      }
      thread_pool.submit([thread_id, work_start, work_end, text, &per_thread_text_utf8] {
        const char *begin = text + work_start;
        const size_t len = work_end - work_start;
        per_thread_text_utf8[thread_id] = vkcom::decode_utf8(begin, begin + len);
      });
      work_start = work_end;
    }

    thread_pool.waitCompletion();
    size_t text_utf8_size = 0;
    for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
      text_utf8_size += per_thread_text_utf8[thread_id].size();
    }
    std::vector<uint32_t> text_utf8(text_utf8_size);
    text_utf8.resize(text_utf8_size);
    work_start = 0;
    for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
      std::vector<uint32_t> &segment = per_thread_text_utf8[thread_id];
      if (!segment.empty()) {
        std::memcpy(text_utf8.data() + work_start,
                    segment.data(),
                    segment.size() * sizeof(uint32_t));
        work_start += segment.size();
      }
    }
    return text_utf8;
  }
}

WordPieceToken::WordPieceToken(const std::string &encoded_word)
 : is_prefix(true), is_special(false), is_malformed(false), word(vkcom::decode_utf8(encoded_word)) {
  if (isSuffixVocab(word)) {
    is_prefix = false;
    word.erase(word.begin(), word.begin() + 2);
  } else if (isSpecialToken(word)) {
    is_special = true;
  }

  bool all_punctuation = true;
  for (uint32_t code_point : word) {
    if (code_point == vkcom::INVALID_UNICODE) {
      is_malformed = true;
    }
    if (!vkcom::is_punctuation(code_point) && !vkcom::is_space(code_point)) {
      all_punctuation = false;
    }
  }
  if (word.empty()) {
    throw std::runtime_error("Vocab word is empty");
  }
  if (is_malformed || (all_punctuation && word.size() > 1)) {
    is_malformed = true;
    std::cerr << "Vocab word is malformed: " << encoded_word << std::endl;
  }
}

WordPieceVocabulary parseVocab(const std::vector<std::string> &vocab) {
  WordPieceVocabulary vocab_utf8;
  vocab_utf8.tokens.reserve(vocab.size());
  int token_id = 0;
  for (const std::string &word : vocab) {
    if (word == kUnkTokenIdStr) {
      vocab_utf8.unk_token_id = token_id;
    }
    WordPieceToken token(word);
    vocab_utf8.tokens.push_back(std::move(token));
    ++token_id;
  }
  return vocab_utf8;
}

WordPieceVocabulary readVocabFromFile(const std::string &file) {
  WordPieceVocabulary vocab_utf8;
  std::ifstream fin(file);
  std::string word;
  int token_id = 0;
  while (std::getline(fin, word)) {
    if (word == kUnkTokenIdStr) {
      vocab_utf8.unk_token_id = token_id;
    }
    WordPieceToken token(word);
    vocab_utf8.tokens.push_back(std::move(token));
    ++token_id;
  }
  return vocab_utf8;
}

bool isSuffixVocab(const std::vector<uint32_t> &word) {
  return word.size() >= 2 && word[0] == vkcom::SHARP_SIGN && word[1] == vkcom::SHARP_SIGN;
}

bool isSpecialToken(const std::vector<uint32_t> &word) {
  return word.size() > 2 && word[0] == static_cast<uint32_t>('[')
      && word.back() == static_cast<uint32_t>(']');
}

} // namespace utils
