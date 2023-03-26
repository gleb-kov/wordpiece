// Copyright (c) 2023 Gleb Koveshnikov

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "src/utils.hpp"
#include "src/word_piece.hpp"

static constexpr int kWordPieceVocabSize = 30'000;
static constexpr int kUnkTokenId = utils::WordPieceVocabulary::kDefaultUnkTokenId;

static int &totalChecks() {
  static int counter = 0;
  return counter;
}

static int &totalPositiveChecks() {
  static int counter = 0;
  return counter;
}

inline bool verifyVocab(const std::vector<std::string> &vocab) {
  std::unordered_set<std::string> vocab_set{vocab.begin(), vocab.end()};
  bool has_empty = (vocab_set.count("") > 0);
  return !vocab.empty() && !has_empty && vocab.size() == vocab_set.size();
}

void assertEq(const std::vector<int> &lhs,
              const std::vector<int> &rhs,
              const std::string &s,
              const std::vector<std::string> &vocab) {
  ++totalChecks();
  if (std::find(lhs.begin(), lhs.end(), kUnkTokenId) != lhs.end()) {
    ++totalPositiveChecks();
  }

  if (lhs != rhs) {
    std::cout << "Comparison failed" << std::endl;

    std::cout << "Test size: " << s.size() << std::endl;
    if (s.size() <= 100) {
      std::cout << s << std::endl;
    }
    std::cout << "Vocab size: " << vocab.size() << std::endl;
    if (vocab.size() <= 20) {
      for (size_t i = 0; i < vocab.size(); i++) {
        std::cout << i << "\"" << vocab[i] << "\"" << std::endl;
      }
    }

    std::cout << "Lhs size: " << lhs.size() << ", rhs size: " << rhs.size() << std::endl;
    size_t index = 0;
    size_t lhs_size = lhs.size();
    size_t rhs_size = rhs.size();
    while (index < lhs_size && index < rhs_size && lhs[index] == rhs[index]) {
      ++index;
    }
    std::cout << "Unmatched index: " << index << std::endl;
    std::cout << "Fragment: " << std::endl;
    index = (index < 10 ? 0ul : index - 10);
    for (int64_t i = 0; i < 10; i++) {
      if (index < lhs_size || index < rhs_size) {
        std::cout << "Index " << index << ", "
                  << (index < lhs_size ? std::to_string(lhs[index]) : "None") << " <> "
                  << (index < rhs_size ? std::to_string(rhs[index]) : "None") << std::endl;
      }
      ++index;
    }
    throw std::runtime_error("Comparison failed");
  }
}

void check(const std::string &s, std::vector<std::string> vocab, const std::vector<int> &expected) {
  if (!verifyVocab(vocab)) {
    throw std::runtime_error("Vocab is malformed");
  }
  std::vector<int> linear = word_piece::linear::encode(s, vocab);
  assertEq(linear, expected, s, vocab);
  std::vector<int> fast = word_piece::fast::encode(s, vocab);
  assertEq(fast, expected, s, vocab);
}

void check(const std::string &s, std::vector<std::string> vocab) {
  if (!verifyVocab(vocab)) {
    throw std::runtime_error("Vocab is malformed");
  }
  std::vector<int> linear = word_piece::linear::encode(s, vocab);
  std::vector<int> fast = word_piece::fast::encode(s, vocab);
  assertEq(linear, fast, s, vocab);
}

std::string randomString(std::mt19937 &rnd, size_t string_length) {
  static constexpr std::string_view kAllChars = "abcdefghijklmnopqrstuvwxyz";
  std::string result;
  result.reserve(string_length);
  while (string_length > 0) {
    --string_length;
    size_t index = std::uniform_int_distribution<size_t>(0ul, kAllChars.size() - 1)(rnd);
    result.push_back(kAllChars[index]);
  }
  return result;
}

std::vector<std::string> randomSplit(const std::string &s, std::mt19937 &rnd, size_t parts) {
  if (s.size() < parts) {
    throw std::runtime_error("Cannot split string");
  }
  std::set<size_t> borders;
  borders.insert(s.size());

  while (borders.size() < parts) {
    size_t index = std::uniform_int_distribution<size_t>(1ul, s.size() - 1)(rnd);
    borders.insert(index);
  }

  std::set<std::string> result;
  size_t start = 0;
  for (size_t next_border : borders) {
    if (start == 0) {
      result.insert(s.substr(start, next_border - start));
    }
    result.insert("##" + s.substr(start, next_border - start));
    start = next_border;
  }

  return std::vector<std::string>(std::make_move_iterator(result.begin()),
                                  std::make_move_iterator(result.end()));
}

void testSimple() {
  check("aaaa", {"aaaa", "aaa", "aa", "a"});
  check("abcdef", {"bcde", "ac", "def", "bc", "bcdef", "a"}, std::vector<int>({kUnkTokenId}));
  check("abcdef", {"bcde", "ac", "def", "bc", "##bcdef", "a"}, std::vector<int>({5, 4}));
  check("   aaaa  ", {"aa", "##aa"}, std::vector<int>({0, 1}));
  check("   aaaa  ", {"aa"}, std::vector<int>({kUnkTokenId}));

  check("aaaa", {"aaaa"}, std::vector<int>({0}));
  check("aaaa", {"##aaaa"}, std::vector<int>({kUnkTokenId}));
  check("aaaa", {"aaaa", "##aaaa", "##aaa", "##aa", "##a"}, std::vector<int>({0}));
  check("aaaa", {"##aaa", "aaaa", "##aa", "##a"}, std::vector<int>({1}));
  check("aaaa", {"aaa", "##aa", "##a", "##aaa"}, std::vector<int>({0, 2}));
  check("aaaa", {"aa", "a", "##aa"}, std::vector<int>({0, 2}));
  check("aaaa", {"aa", "a", "##aaa"}, std::vector<int>({kUnkTokenId}));
  check("aaaa", {"aa", "##a"}, std::vector<int>({0, 1, 1}));

  check("abcdef", {"##def", "abc"}, std::vector<int>({1, 0}));
  check("abcdef",
        {"##bcde", "##ac", "##def", "##bc", "##bcdef", "a", "##a"},
        std::vector<int>({5, 4}));
  check("abcdef", {"##bcdd", "##ac", "##def", "##bc", "##bcdff", "a"}, std::vector<int>({5, 3, 2}));

  check("djzhoyuhmcij",
        {"d", "##j", "##z", "##h", "##o", "##y", "##u", "##m", "##c", "##i", "##d"},
        std::vector<int>({0, 1, 2, 3, 4, 5, 6, 3, 7, 8, 9, 1}));
}

void testPunctuation() {
  check("self-made", {"self", "made", "-", "##-", "##made"}, std::vector<int>({0, 2, 1}));
  check("self, made", {"self", "made", ",", "##,", "##made"}, std::vector<int>({0, 2, 1}));
  check("self  , made", {"self", "made", ",", "##,", "##made"}, std::vector<int>({0, 2, 1}));
}

void testNonSplitted() {
  check("abc", {"a", "abd"}, std::vector<int>({kUnkTokenId}));
  check("abc a abc abd", {"a", "abd"}, std::vector<int>({kUnkTokenId, 0, kUnkTokenId, 1}));
  check("abcdef",
        {"bcde", "ac", "def", "bc", "bcdef", "##a", "##b", "##c", "##d"},
        std::vector<int>({kUnkTokenId}));
}

void testMaxMatch() {
  // NB, this considered as MaxMatch algorithm
  check("abcdef",
        {"a", "##bcdef", "ab", "##c", "##d", "##e", "##f"},
        std::vector<int>({2, 3, 4, 5, 6}));

  check("abcdef abc abcd", {"abcd", "def", "abc"}, std::vector<int>({kUnkTokenId, 2, 0}));

  check("djzhoyuhmcijprfwrssuhvgzw",
        {"##c",
         "d",
         "##d"
         "##f",
         "##g",
         "##h",
         "##hv",
         "##i",
         "##j",
         "##m",
         "##o",
         "##p",
         "##r",
         "##s",
         "##u",
         "##uh",
         "##w",
         "##y",
         "##z"});
}

void testUtf8() {
  check("привет мир", {"привет", "мир"}, std::vector<int>({0, 1}));
  check("привет мир", {"при", "##вет", "мир"}, std::vector<int>({0, 1, 2}));
  check("токенизация это круто",
        {"ток", "крут", "это", "##за", "##ция", "ция"},
        std::vector<int>({kUnkTokenId, 2, kUnkTokenId}));
  check("токенизация это круто",
        {"ток", "крут", "это", "##за", "##ени", "##о", "##ция", "ция"},
        std::vector<int>({0, 4, 3, 6, 2, 1, 5}));
}

void testRandomSplit(size_t text_len_from,
                     size_t text_len_to,
                     size_t text_len_step,
                     size_t parts_from,
                     size_t parts_to,
                     bool positive,
                     bool verbose = false) {
  std::mt19937 rnd(17);
  for (size_t text_len = text_len_from; text_len <= text_len_to; text_len += text_len_step) {
    for (size_t parts = std::min(text_len, parts_from); parts <= std::min(text_len, parts_to);
         parts++) {
      if (verbose) {
        std::cout << "running testRandomSplit, text_len " << text_len << ", vocab_size " << parts
                  << std::endl;
      }

      for (int i = 0; i < 3; i++) {
        std::string sample = randomString(rnd, text_len);
        std::vector<std::string> split = randomSplit(sample, rnd, parts);
        if (!positive) {
          split.erase(split.begin());
        }

        check(sample, std::move(split));
      }
    }
  }
}

int main() {
  std::cout << "running small unit tests." << std::endl;
  testSimple();
  testNonSplitted();
  testPunctuation();
  testMaxMatch();
  testUtf8();

  std::cout << "running stress tests (split)." << std::endl;
  testRandomSplit(10, 300, 5, 2, 100, true);
  testRandomSplit(10, 300, 5, 2, 100, false);
  testRandomSplit(100'000,
                  1'000'000,
                  400'000,
                  kWordPieceVocabSize,
                  kWordPieceVocabSize,
                  true,
                  true);
  testRandomSplit(10'000'000,
                  10'000'000,
                  200'000,
                  kWordPieceVocabSize,
                  kWordPieceVocabSize,
                  true,
                  true);

  std::cout << "Tests are finished. Passed " << totalChecks() << " checks, including "
            << totalPositiveChecks() << " positive." << std::endl;
}
