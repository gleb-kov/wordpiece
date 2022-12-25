#include <chrono>
#include <random>
#include <vector>
#include <string>
#include <string_view>

#include "naive.hpp"
#include "word_piece.hpp"

inline bool verifyVocab(const std::vector<std::string_view> &vocab) {
    std::unordered_set<std::string_view> vocab_set{vocab.begin(), vocab.end()};
    bool has_empty = (vocab_set.count("") > 0);
    return !vocab.empty() && !has_empty && vocab.size() == vocab_set.size();
}

void assertEq(const std::vector<int> &lhs, const std::vector<int> &rhs) {
    if (lhs != rhs) {
        throw std::runtime_error("Comparison failed");
    }
}

void check(const std::string_view s, const std::vector<std::string> &vocab) {
    assert(verifyVocab(vocab));
    std::vector<int> fast = word_piece::wordPiece(s, vocab);
    std::vector<int> naive = naiveTokenization(s, vocab);
    assert_eq(fast, naive);
}

std::string randomString(std::mt19937 &rnd, int len) {
    static constexpr std::string_view kAllChars = "abcdefghijklmnopqrstuvwxyz";
    std::string result;
    result.reserve(len);
    while (len-- > 0) {
        int index = std::uniform_int_distribution(0, static_cast<int>(kAllChars.size() - 1))(rnd);
        result.push_back(kAllChars[index]);
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
    assertEq(word_piece::wordPiece("abcdef", {"bcde", "ac", "def", "bc", "bcdef", "a"}), std::vector<int>({5, 4}));
    assertEq(word_piece::wordPiece("abcdef", {"bcdd", "ac", "def", "bc", "bcdff", "a"}),
              std::vector<int>({5, 3, 2}));
}

void testNonSplitted() {
    assertEq(word_piece::wordPiece("abc", {"a", "abd"}), std::vector<int>());
    assertEq(word_piece::wordPiece("abcdef", {"bcde", "ac", "def", "bc", "bcdef"}), std::vector<int>({}));
}

void testMaxMatch() {
    // NB, this considered as MaxMatch algorithm
    assertEq(word_piece::wordPiece("abcdef", {"a", "bcdef", "ab", "c", "d", "e", "f"}),
              std::vector<int>({2, 3, 4, 5, 6}));

    assertEq(word_piece::wordPiece("abcdef", {"abcd", "def", "abc"}), std::vector<int>({}));
}

void testRandomSplit(int str_len_from, int str_len_to, int str_len_step, int parts_from, int parts_to, bool positive) {
    std::mt19937 rnd(17);
    for (int str_len = str_len_from; str_len <= str_len_to; str_len += str_len_step) {
        for (int parts = std::min(str_len, parts_from); parts <= std::min(str_len, parts_to); parts++) {
            for (int i = 0; i < 30; i++) {
                std::string sample = randomString(rnd, str_len);
                std::vector<std::string> split = randomSplit(sample, rnd, parts);
                if (!positive) {
                    split.erase(split.begin());
                }

                check(sample, split);
            }
        }
    }
}

int main() {
    testSimple();
    testNonSplitted();
    testMaxMatch();
    testRandomSplit(10, 100, 5, 2, 100, true);
    testRandomSplit(10, 100, 5, 2, 100, false);
    testRandomSplit(10000, 100000, 10000, 30000, 30000, true);
}