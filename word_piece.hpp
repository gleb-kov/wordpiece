#ifndef WORD_PIECE_H
#define WORD_PIECE_H

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <numeric>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "utf8.hpp"

namespace word_piece {

namespace detail {

inline int64_t currentTs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

namespace suf_array3n {

// http://www.cs.cmu.edu/~guyb/paralg/papers/KarkkainenSanders03.pdf

// always_inline?
template <typename Char, typename Count>
inline bool leq(Char a1, Count a2, Char b1, Count b2) {
    return (a1 < b1 || (a1 == b1 && a2 <= b2));
}

// always_inline?
template <typename Char, typename Count>
inline bool leq(Char a1, Char a2, Count a3, Char b1, Char b2, Count b3) {
    return (a1 < b1 || (a1 == b1 && leq(a2, a3, b2, b3)));
}

template <typename Char, typename Count>
class RadixPassSolver {
public:
    explicit RadixPassSolver(size_t alphabet_size) {
        size_t thread_count = static_cast<size_t>(std::thread::hardware_concurrency());
        if (thread_count == 0) {
            thread_count = 8;
        }
        threads_.reserve(thread_count);
        count_.assign(thread_count + 1, std::vector<Count>(alphabet_size + 1, 0));
    }

    // stably sort a[0..n-1] to b[0..n-1] with keys in 0..alphabet_size from r
    void sort(Count *a, Count *b, Char *r, size_t n, size_t alphabet_size) {
        if (count_[0].size() < alphabet_size + 1) {
            for (size_t i = 0; i < count_.size(); i++){
                count_[i].assign(alphabet_size + 1, 0);
            }
        } else {
            for (size_t i = 0; i < count_.size(); i++) {
                std::memset(count_[i].data(), 0, (alphabet_size + 1) * sizeof(Count));
            }
        }

        if (n <= kWorkBatch) {
            // single thread sort
            fillCount(1, a, r, 0, n);
            makePrefixSum(alphabet_size);
            sortImpl(a, b, r, 0, n);
        } else {
            const size_t thread_cnt = std::min(threads_.size(), n / kWorkBatch);
            const size_t work_batch = n / thread_cnt;
            (void)(work_batch);
            fillCount(1, a, r, 0, n);
            makePrefixSum(alphabet_size);
            sortImpl(a, b, r, 0, n);
        }
    }

    ~RadixPassSolver() {
        for (std::thread &t : threads_) {
            t.join();
        }
    }

private:
    void fillCount(size_t thread_id, Count *a, Char *r, size_t begin, size_t end) {
        auto &count = count_[thread_id];
        for (size_t i = begin; i < end; i++) {
            count[r[a[i]]]++;
        }
    }

    void makePrefixSum(size_t alphabet_size) {
        Count sum = 0;
        for (size_t i = 0; i <= alphabet_size; i++) { // exclusive prefix sums
            Count item_count = 0;
            for (size_t j = 1; j < count_.size(); j++) {
                item_count += count_[j][i];
            }
            count_[0][i] = sum;
            sum += item_count;
        }
    }

    void sortImpl(Count *a, Count *b, Char *r, size_t begin, size_t end) {
        auto &count = count_[0];
        for (size_t i = begin; i < end; i++) {
            b[count[r[a[i]]]++] = a[i];
        }
    }

private:
    static constexpr size_t kWorkBatch = 2'500'000;
    enum WorkType : int {
        Idle,
        FillCount,
        Sort,
    };

    struct Work {
        WorkType type;
        size_t begin;
        size_t end;
    };


    std::vector<std::vector<Count>> count_;
    std::vector<std::thread> threads_;
};

// stably sort a[0..n-1] to b[0..n-1] with keys in 0..alphabet_size from r
template <typename Char, typename Count>
inline void radixPass(Count *a, Count *b, Char *r, size_t n, size_t alphabet_size) {
    static RadixPassSolver<Char, Count> solver(alphabet_size);
    solver.sort(a, b, r, n, alphabet_size);
}

// stably sort a[0..n-1] to b[0..n-1] with keys in 0..alphabet_size from r
template <typename Char, typename Count>
inline void radixPass_(Count *a, Count *b, Char *r, size_t n, size_t alphabet_size) {
    std::vector<Count> count(alphabet_size + 1, 0);
    for (size_t i = 0; i < n; i++) {
        count[r[a[i]]]++;
    }
    Count sum = 0;
    for (size_t i = 0; i <= alphabet_size; i++) { // exclusive prefix sums
        Count t = count[i];
        count[i] = sum;
        sum += t;
    }
    for (size_t i = 0; i < n; i++) {
        b[count[r[a[i]]]++] = a[i];
    }
}

// find the suffix array SA of s[0..n-1] in {1..alphabet_size}?n
// require s[n]=s[n+1]=s[n+2]=0, n>=2
template <typename Char, typename Count>
inline void suffixArray(Char *s, Count *SA, size_t n, size_t alphabet_size) {
    Count n0 = static_cast<Count>((n + 2) / 3);
    Count n1 = static_cast<Count>((n + 1) / 3);
    size_t n2 = n / 3;
    size_t n02 = static_cast<size_t>(n0) + n2;

    Count *s12 = new Count[n02 + 3];
    s12[n02] = s12[n02 + 1] = s12[n02 + 2] = 0;
    Count *SA12 = new Count[n02 + 3];
    SA12[n02] = SA12[n02 + 1] = SA12[n02 + 2] = 0;
    Count *s0 = new Count[static_cast<size_t>(n0)];
    Count *SA0 = new Count[static_cast<size_t>(n0)];
    // generate positions of mod 1 and mod 2 suffixes
    // the "+(n0-n1)" adds a dummy mod 1 suffix if n%3 == 1

    for (size_t i = 0, j = 0; i < n + static_cast<size_t>(n0 - n1); i++) {
        if (i % 3 != 0) {
            s12[j++] = static_cast<Count>(i);
        }
    }
    // lsb radix sort the mod 1 and mod 2 triples
    radixPass(s12, SA12, s + 2, n02, alphabet_size);
    radixPass(SA12, s12, s + 1, n02, alphabet_size);
    radixPass(s12, SA12, s, n02, alphabet_size);
    // find lexicographic names of triples
    size_t name = 0;
    Char c0 = std::numeric_limits<Char>::max();
    Char c1 = c0;
    Char c2 = c1;

    for (size_t i = 0; i < static_cast<size_t>(n02); i++) {
        if (s[SA12[i]] != c0 || s[SA12[i] + 1] != c1 || s[SA12[i] + 2] != c2) {
            name++;
            c0 = s[SA12[i]];
            c1 = s[SA12[i] + 1];
            c2 = s[SA12[i] + 2];
        }
        size_t half = static_cast<size_t>(SA12[i] % 3 == 1 ? 0 : n0);
        s12[static_cast<size_t>(SA12[i] / 3) + half] = static_cast<Count>(name);
    }
    // recurse if names are not yet unique
    if (name < n02) {
        suffixArray<Count, Count>(s12, SA12, n02, name);
        // store unique names in s12 using the suffix array
        for (size_t i = 0; i < static_cast<size_t>(n02); i++) {
            s12[SA12[i]] = static_cast<Count>(i + 1);
        }
    } else { // generate the suffix array of s12 directly
        for (size_t i = 0; i < static_cast<size_t>(n02); i++) {
            SA12[s12[i] - 1] = static_cast<Count>(i);
        }
    }
    // stably sort the mod 0 suffixes from SA12 by their first character
    for (size_t i = 0, j = 0; i < static_cast<size_t>(n02); i++) {
        if (SA12[i] < n0) {
            s0[j++] = 3 * SA12[i];
        }
    }
    radixPass(s0, SA0, s, static_cast<size_t>(n0), alphabet_size);
    // merge sorted SA0 suffixes and sorted SA12 suffixes

    Count p = 0;
    for (size_t k = 0, t = static_cast<size_t>(n0 - n1); k < n; k++) {
#define GetI() (SA12[t] < n0 ? SA12[t] * 3 + 1 : (SA12[t] - n0) * 3 + 2)
        Count j = SA0[p];  // pos of current offset 0 suffix
        Count i = GetI();  // pos of current offset 12 suffix
        if (SA12[t] < n0 ? // different compares for mod 1 and mod 2 suffixes
                leq(s[i], s12[SA12[t] + n0], s[j], s12[j / 3])
                         : leq(s[i],
                               s[i + 1],
                               s12[SA12[t] - n0 + 1],
                               s[j],
                               s[j + 1],
                               s12[j / 3 + n0])) { // suffix from SA12 is smaller
            SA[k] = i;
            t++;
            if (t == n02) { // done --- only SA0 suffixes left
                k++;
                for (; p < n0; p++, k++) {
                    SA[k] = SA0[p];
                }
            }
        } else { // suffix from SA0 is smaller
            SA[k] = j;
            p++;
            if (p == n0) { // done --- only SA12 suffixes left
                k++;
                for (; t < n02; t++, k++) {
                    SA[k] = GetI();
                }
            }
        }
    }
    delete[] s12;
    delete[] SA12;
    delete[] SA0;
    delete[] s0;
}

} // namespace suf_array3n

template <typename Count>
inline std::vector<Count>
calcLcp(const uint32_t *str, const Count *suf_a, const std::vector<Count> &suf_array_index) {
    std::vector<Count> lcp(suf_array_index.size() - 1);
    size_t prefix_len = 0;
    for (size_t i = 0; i < suf_array_index.size(); i++) {
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

    return lcp;
}

} // namespace detail

template <typename Count>
inline std::vector<int> wordPiece(const std::vector<uint32_t> &text,
                                  const std::vector<std::vector<uint32_t>> &vocab,
                                  int unk_token_id,
                                  size_t total_length,
                                  size_t longest_word_vocab) {
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
        assert(pos == total_length);
    }

    S[total_length] = S[total_length + 1] = S[total_length + 2] = 0;
    Count *suf = new Count[total_length + 3];
    detail::suf_array3n::suffixArray<uint32_t, Count>(S, suf, total_length, alphabet_size);

    std::vector<Count> suf_array_index(total_length);
    for (size_t i = 0; i < total_length; i++) {
        suf_array_index[static_cast<size_t>(suf[i])] = static_cast<Count>(i);
    }

    std::vector<Count> lcp = detail::calcLcp<Count>(S, suf, suf_array_index);
    delete[] S;
    delete[] suf;

    static constexpr int kNoWord = -1;
    std::vector<int> who(total_length, kNoWord);

    size_t vocab_start_pos = text.size() + 1;
    for (size_t i = 0; i < vocab.size(); i++) {
        who[static_cast<size_t>(suf_array_index[vocab_start_pos])] = static_cast<int>(i);
        vocab_start_pos += vocab[i].size() + 1;
    }
    const auto get_closest = [&lcp, &who, &vocab, longest_word_vocab, total_length]() {
        std::vector<int> result(total_length, kNoWord);
        // (i, |i|); i is index in ts
        std::vector<std::pair<int, Count>> st;
        st.reserve(longest_word_vocab);
        for (size_t i = 0; i < total_length; i++) {
            if (i > 0) {
                while (!st.empty() && st.back().second > lcp[i - 1]) {
                    st.pop_back();
                }
            }
            if (who[i] != kNoWord) {
                Count len = static_cast<Count>(vocab[static_cast<size_t>(who[i])].size());
                assert(st.empty() || st.back().second < len);
                st.emplace_back(who[i], len);
            }
            if (!st.empty()) {
                result[i] = st.back().first;
            }
        }
        return result;
    };

    const std::vector<int> L = get_closest();
    std::reverse(who.begin(), who.end());
    std::reverse(lcp.begin(), lcp.end());
    const std::vector<int> R = get_closest();

    std::vector<int> token_ids;
    token_ids.reserve(text.size() * vocab.size() / (total_length - text.size()));

    size_t match_index = 0;
    while (match_index < text.size()) {
        size_t id = static_cast<size_t>(suf_array_index[match_index]);
        int x = L[id];
        int y = R[total_length - 1 - id];

        if (x != kNoWord || y != kNoWord) {
            int token_id;
            if (x != kNoWord && y != kNoWord) {
                token_id
                    = vocab[static_cast<size_t>(x)].size() > vocab[static_cast<size_t>(y)].size()
                        ? x
                        : y;
            } else {
                token_id = std::max(x, y);
            }
            token_ids.push_back(token_id);
            match_index += vocab[static_cast<size_t>(token_id)].size();
        } else {
            token_ids.push_back(unk_token_id);
            while (match_index != text.size() && !vkcom::is_space(text[match_index])) {
                ++match_index;
            }
        }
        while (match_index != text.size() && vkcom::is_space(text[match_index])) {
            ++match_index;
        }
    }

    assert(match_index == text.size());
    return token_ids;
}

inline std::vector<int>
wordPiece(const std::string &text, const std::vector<std::string> &vocab, int unk_token_id = -1) {
    if (text.empty()) {
        return {};
    } 
    std::vector<uint32_t> text_utf8 = vkcom::decode_utf8(text);
    std::vector<std::vector<uint32_t>> vocab_utf8(vocab.size());

    size_t total_length = text_utf8.size() + 1;
    size_t longest_word_vocab = 1;

    for (size_t i = 0; i < vocab.size(); i++) {
        vocab_utf8[i] = vkcom::decode_utf8(vocab[i]);
        total_length += vocab_utf8[i].size() + 1;
        longest_word_vocab = std::max(longest_word_vocab, vocab_utf8[i].size());
    }

    // gives 6% speed boost due to cache and alloc optimizations.
    if (total_length < 2'000'000'000) {
        return wordPiece<uint32_t>(text_utf8, vocab_utf8, unk_token_id, total_length, longest_word_vocab);
    } else {
        return wordPiece<size_t>(text_utf8, vocab_utf8, unk_token_id, total_length, longest_word_vocab);
    }
}

} // namespace word_piece

#endif // WORD_PIECE_H
