// Copyright (c) 2023 Gleb Koveshnikov

#pragma once

#include <string>
#include <vector>

#include "third_party/thread_pool.hpp"

namespace utils {

int64_t currentTs();

ThreadPool &globalThreadPool(size_t n_threads = 0);

void writeToFile(const std::string &file, const std::vector<int> &ids);

std::vector<uint32_t> parseText(const char *text, size_t size, ThreadPool &thread_pool);

struct WordPieceToken {
  explicit WordPieceToken(const std::string &encoded_word);

  bool is_prefix;
  std::vector<uint32_t> word;
};

struct WordPieceVocabulary {
  static constexpr int kDefaultUnkTokenId = -1;

  std::vector<WordPieceToken> tokens;
  int unk_token_id = kDefaultUnkTokenId;
};

WordPieceVocabulary parseVocab(const std::vector<std::string> &vocab);

WordPieceVocabulary readVocabFromFile(const std::string &file);

bool isSuffixVocab(const std::vector<uint32_t> &word);

bool isSuffixVocab(const uint32_t *begin, const uint32_t *end);

} // namespace utils
