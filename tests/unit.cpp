#include <gtest/gtest.h>

#include "wordpiece.hpp"

TEST(Unit, Simple) {
    ASSERT_EQ(maxMatch("aaaa", {"aaaa"}), std::vector<int>({0}));
    ASSERT_EQ(maxMatch("aaaa", {"aaaa", "aaa", "aa", "a"}), std::vector<int>({0}));
    ASSERT_EQ(maxMatch("aaaa", {"aaa", "aaaa", "aa", "a"}), std::vector<int>({1}));
    ASSERT_EQ(maxMatch("aaaa", {"aaa", "aa", "a"}), std::vector<int>({0, 2}));
    ASSERT_EQ(maxMatch("aaaa", {"aa", "a"}), std::vector<int>({0, 0}));

    ASSERT_EQ(maxMatch("abcdef", {"def", "abc"}), std::vector<int>({1, 0}));
    ASSERT_EQ(maxMatch("abcdef", {"bcde", "ac", "def", "bc", "bcdef", "a"}), std::vector<int>({5, 4}));
    ASSERT_EQ(maxMatch("abcdef", {"bcdd", "ac", "def", "bc", "bcdff", "a"}),
              std::vector<int>({5, 3, 2}));
}

TEST(Unit, NonSplitted) {
    ASSERT_EQ(maxMatch("abc", {"a", "abd"}), std::vector<int>());
    ASSERT_EQ(maxMatch("abcdef", {"bcde", "ac", "def", "bc", "bcdef"}), std::vector<int>({}));
}

TEST(Unit, MaxMatch) {
    // NB, this considered as MaxMatch algorithm
    ASSERT_EQ(maxMatch("abcdef", {"a", "bcdef", "ab", "c", "d", "e", "f"}),
              std::vector<int>({2, 3, 4, 5, 6}));

    ASSERT_EQ(maxMatch("abcdef", {"abcd", "def", "abc"}), std::vector<int>({}));
}
