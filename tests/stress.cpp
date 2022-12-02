#include <chrono>
#include <gtest/gtest.h>
#include <random>

#include "wordpiece.hpp"

std::vector<int> naiveTokenization(const std::string &s, const std::vector<std::string> &vocab) {
    std::unordered_map<std::string_view, int> word_to_id;
    for (int i = 0; i < static_cast<int64_t>(vocab.size()); i++) {
        assert(word_to_id.count(vocab[i]) == 0);
        word_to_id[vocab[i]] = i;
    }

    size_t start = 0;
    size_t end = s.size();

    std::vector<int> tokens;
    while (start < end) {
        std::string test(s.c_str() + start, end - start);

        auto it = word_to_id.find(test);
        if (it != word_to_id.end()) {
            tokens.push_back(it->second);
            start = end;
            end = s.size();
        } else {
            end -= 1;
            if (start == end) {
                return {};
            }
        }
    }

    return tokens;
}

void compare(const std::string &s, const std::vector<std::string> &vocab, bool print_perf = false) {
    assert(verifyVocab(vocab));
    auto start = currentTs();
    std::vector<int> fast = maxMatch(s, vocab);
    auto between = currentTs();
    std::vector<int> naive = naiveTokenization(s, vocab);
    auto after = currentTs();

    if (print_perf) {
        double perf = static_cast<double>(after - between) / static_cast<double>(between - start);
        std::cout << s.size() << ' ' << vocab.size() << ' ' << perf << ' ' << between - start
                  << std::endl;
    }

    ASSERT_EQ(fast, naive);
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

TEST(Stress, Simple) {
    compare("aaaa", {"aaaa", "aaa", "aa", "a"});
    compare("abcdef", {"bcde", "ac", "def", "bc", "bcdef", "a"});
}

TEST(Stress, RandomSplit) {
    std::mt19937 rnd(17);
    for (int str_len = 5; str_len <= 100; str_len += 5) {
        for (int parts = 2; parts <= str_len; parts++) {
            for (int i = 0; i < 30; i++) {
                std::string sample = randomString(rnd, str_len);
                std::vector<std::string> split = randomSplit(sample, rnd, parts);

                compare(sample, split);
            }
        }
    }
}

TEST(Stress, RandomSplitNegative) {
    std::mt19937 rnd(127);
    for (int str_len = 5; str_len <= 100; str_len += 5) {
        for (int parts = 2; parts <= str_len; parts++) {
            for (int i = 0; i < 30; i++) {
                std::string sample = randomString(rnd, str_len);
                std::vector<std::string> split = randomSplit(sample, rnd, parts);
                split.erase(split.end());

                compare(sample, split);
            }
        }
    }
}

TEST(Stress, Perf) {
    std::mt19937 rnd(17);
    for (int str_len = 10000; str_len <= 10000; str_len += 5) {
        for (int parts = 3000; parts <= 3000; parts++) {
            for (int i = 0; i < 3; i++) {
                std::string sample = randomString(rnd, str_len);
                std::vector<std::string> split = randomSplit(sample, rnd, parts);
                /*std::cout << sample << std::endl;
                for (const auto &t : split) {
                    std::cout << "\"##" << t << "\", ";
                }
                std::cout << std::endl;*/

                compare(sample, split, true);
            }
        }
    }
}
