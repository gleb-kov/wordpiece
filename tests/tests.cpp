#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "../word_piece.hpp"
#include "naive.hpp"

static int &totalChecks() {
    static int counter = 0;
    return counter;
}

inline bool verifyVocab(const std::vector<std::string> &vocab) {
    std::unordered_set<std::string> vocab_set{vocab.begin(), vocab.end()};
    bool has_empty = (vocab_set.count("") > 0);
    return !vocab.empty() && !has_empty && vocab.size() == vocab_set.size();
}

void assertEq(const std::vector<int> &lhs, const std::vector<int> &rhs) {
    ++totalChecks();
    if (lhs != rhs) {
        throw std::runtime_error("Comparison failed");
    }
}

void check(const std::string_view s, const std::vector<std::string> &vocab, bool verbose = false) {
    assert(verifyVocab(vocab));
    auto start_ts = word_piece::detail::currentTs();
    std::vector<int> fast = word_piece::wordPiece(s, vocab);
    auto between_ts = word_piece::detail::currentTs();
    std::vector<int> naive = naiveTokenization(s, vocab);
    auto after_ts = word_piece::detail::currentTs();
    assertEq(fast, naive);

    if (verbose) {
        auto fast_ts = between_ts - start_ts;
        auto naive_ts = after_ts - between_ts;
        std::cout << std::fixed << std::setprecision(2) << "Check passed; perf "
                  << fast_ts / 1'000'000 << "ms"
                  << ", boost is " << static_cast<double>(naive_ts) / static_cast<double>(fast_ts)
                  << " times" << std::endl;
    }
}

std::string randomString(std::mt19937 &rnd, int string_length) {
    static constexpr std::string_view kAllChars = "abcdefghijklmnopqrstuvwxyz";
    std::string result;
    result.reserve(string_length);
    while (string_length-- > 0) {
        int index = std::uniform_int_distribution(0, static_cast<int>(kAllChars.size() - 1))(rnd);
        result.push_back(kAllChars[index]);
    }
    return result;
}

std::vector<std::string> randomStringSet(std::mt19937 &rnd, int string_count, int max_string_len) {
    std::set<std::string> result;
    while (string_count > static_cast<int>(result.size())) {
        int len = std::uniform_int_distribution(4, max_string_len)(rnd);
        result.insert(randomString(rnd, len));
    }

    return std::vector<std::string>(std::make_move_iterator(result.begin()),
                                    std::make_move_iterator(result.end()));
}

std::string randomStringFromSet(std::mt19937 &rnd,
                                int string_length,
                                const std::vector<std::string> &string_set) {
    std::string result;
    result.reserve(string_length + 100);
    const int right_index = static_cast<int>(string_set.size()) - 1;
    while (string_length > static_cast<int>(result.size())) {
        int index = std::uniform_int_distribution(0, right_index)(rnd);
        result.append(string_set[index]);
    }
    return result;
}

std::vector<std::string> randomSplit(const std::string &s, std::mt19937 &rnd, int parts) {
    assert(static_cast<int>(s.size()) >= parts);
    std::set<int> borders;
    borders.insert(static_cast<int>(s.size()));

    while (static_cast<int>(borders.size()) < parts) {
        int index = std::uniform_int_distribution(1, static_cast<int>(s.size() - 1))(rnd);
        borders.insert(index);
    }

    std::set<std::string> result;
    int start = 0;
    for (int next_border : borders) {
        result.insert(s.substr(start, next_border - start));
        start = next_border;
    }

    return std::vector<std::string>(std::make_move_iterator(result.begin()),
                                    std::make_move_iterator(result.end()));
}

void testSimple() {
    check("aaaa", {"aaaa", "aaa", "aa", "a"});
    check("abcdef", {"bcde", "ac", "def", "bc", "bcdef", "a"});

    assertEq(word_piece::wordPiece("aaaa", {"aaaa"}), std::vector<int>({0}));
    assertEq(word_piece::wordPiece("aaaa", {"aaaa", "aaa", "aa", "a"}), std::vector<int>({0}));
    assertEq(word_piece::wordPiece("aaaa", {"aaa", "aaaa", "aa", "a"}), std::vector<int>({1}));
    assertEq(word_piece::wordPiece("aaaa", {"aaa", "aa", "a"}), std::vector<int>({0, 2}));
    assertEq(word_piece::wordPiece("aaaa", {"aa", "a"}), std::vector<int>({0, 0}));

    assertEq(word_piece::wordPiece("abcdef", {"def", "abc"}), std::vector<int>({1, 0}));
    assertEq(word_piece::wordPiece("abcdef", {"bcde", "ac", "def", "bc", "bcdef", "a"}),
             std::vector<int>({5, 4}));
    assertEq(word_piece::wordPiece("abcdef", {"bcdd", "ac", "def", "bc", "bcdff", "a"}),
             std::vector<int>({5, 3, 2}));
}

void testNonSplitted() {
    assertEq(word_piece::wordPiece("abc", {"a", "abd"}), std::vector<int>());
    assertEq(word_piece::wordPiece("abcdef", {"bcde", "ac", "def", "bc", "bcdef"}),
             std::vector<int>({}));
}

void testMaxMatch() {
    // NB, this considered as MaxMatch algorithm
    assertEq(word_piece::wordPiece("abcdef", {"a", "bcdef", "ab", "c", "d", "e", "f"}),
             std::vector<int>({2, 3, 4, 5, 6}));

    assertEq(word_piece::wordPiece("abcdef", {"abcd", "def", "abc"}), std::vector<int>({}));
}

void testRandomSplit(int text_len_from,
                     int text_len_to,
                     int text_len_step,
                     int parts_from,
                     int parts_to,
                     bool positive,
                     bool verbose = false) {
    std::mt19937 rnd(17);
    for (int text_len = text_len_from; text_len <= text_len_to; text_len += text_len_step) {
        for (int parts = std::min(text_len, parts_from); parts <= std::min(text_len, parts_to);
             parts++) {
            if (verbose) {
                std::cout << "running testRandomSplit, text_len " << text_len << ", vocab_size "
                          << parts << std::endl;
            }

            for (int i = 0; i < 3; i++) {
                std::string sample = randomString(rnd, text_len);
                std::vector<std::string> split = randomSplit(sample, rnd, parts);
                if (!positive) {
                    split.erase(split.begin());
                }

                check(sample, split, verbose);
            }
        }
    }
}

void testRandomConcat(int text_len_from,
                      int text_len_to,
                      int text_len_step,
                      int parts_from,
                      int parts_to,
                      int max_part_len,
                      bool positive,
                      bool verbose = false) {
    std::mt19937 rnd(17);

    for (int text_len = text_len_from; text_len <= text_len_to; text_len += text_len_step) {
        for (int parts = std::min(text_len, parts_from); parts <= std::min(text_len, parts_to);
             parts++) {
            if (verbose) {
                std::cout << "running testRandomConcat, text_len " << text_len << ", vocab_size "
                          << parts << std::endl;
            }

            for (int i = 0; i < 3; i++) {
                std::vector<std::string> string_set = randomStringSet(rnd, parts, max_part_len);
                std::string sample = randomStringFromSet(rnd, text_len, string_set);
                if (!positive) {
                    string_set.erase(string_set.begin());
                }

                check(sample, string_set, verbose);
            }
        }
    }
}

int main() {
    std::cout << "running small unit tests." << std::endl;
    testSimple();
    testNonSplitted();
    testMaxMatch();

    std::cout << "running stress tests." << std::endl;
    testRandomSplit(10, 100, 5, 2, 100, true);
    testRandomSplit(10, 100, 5, 2, 100, false);
    testRandomSplit(100'000, 1'000'000, 200'000, 30'000, 30'000, true, true);

    testRandomConcat(10, 100, 5, 2, 100, 10, true);
    testRandomConcat(10, 100, 5, 2, 100, 10, false);
    testRandomConcat(100'000, 1'000'000, 100'000, 30'000, 30'000, 18, true, true);

    std::cout << "Tests are finished. Passed " << totalChecks() << " checks.";
}
