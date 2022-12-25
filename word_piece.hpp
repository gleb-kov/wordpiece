#ifndef WORD_PIECE_H
#define WORD_PIECE_H

#include <chrono>
#include <string>
#include <string_view>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <utility>
#include <unordered_set>
#include <cassert>

namespace word_piece {

namespace detail {

inline int64_t currentTs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

namespace suf_array3n {

inline bool leq(int a1, int a2, int b1, int b2) { return (a1 < b1 || (a1 == b1 && a2 <= b2)); }

inline bool leq(int a1, int a2, int a3, int b1, int b2, int b3) {
    return (a1 < b1 || (a1 == b1 && leq(a2, a3, b2, b3)));
}

// stably sort a[0..n-1] to b[0..n-1] with keys in 0..K from r
inline void radixPass(int *a, int *b, int *r, int n, int K) { // count occurrences
    int *c = new int[K + 1];                                  // counter array
    std::memset(c, 0, K * sizeof(int));
    for (int i = 0; i < n; i++) {
        c[r[a[i]]]++; // count occurrences
    }
    for (int i = 0, sum = 0; i <= K; i++) { // exclusive prefix sums
        int t = c[i];
        c[i] = sum;
        sum += t;
    }
    for (int i = 0; i < n; i++) {
        b[c[r[a[i]]]++] = a[i]; // sort
    }
    delete[] c;
}

// find the suffix array SA of s[0..n-1] in {1..K}?n
// require s[n]=s[n+1]=s[n+2]=0, n>=2
inline void suffixArray(int *s, int *SA, int n, int K) {
    int n0 = (n + 2) / 3, n1 = (n + 1) / 3, n2 = n / 3, n02 = n0 + n2;
    int *s12 = new int[n02 + 3];
    s12[n02] = s12[n02 + 1] = s12[n02 + 2] = 0;
    int *SA12 = new int[n02 + 3];
    SA12[n02] = SA12[n02 + 1] = SA12[n02 + 2] = 0;
    int *s0 = new int[n0];
    int *SA0 = new int[n0];
    // generate positions of mod 1 and mod 2 suffixes
    // the "+(n0-n1)" adds a dummy mod 1 suffix if n%3 == 1

    // n%3==0 @ 0 + 0 = 0
    // n%3==1 @ 1 + (1 - 0) = 2
    // n%3==2 @ 2 + (1 - 1) = 2
    for (int i = 0, j = 0; i < n + (n0 - n1); i++) {
        if (i % 3 != 0) {
            s12[j++] = i;
        }
    }
    // lsb radix sort the mod 1 and mod 2 triples
    radixPass(s12, SA12, s + 2, n02, K);
    radixPass(SA12, s12, s + 1, n02, K);
    radixPass(s12, SA12, s, n02, K);
    // find lexicographic names of triples
    int name = 0, c0 = -1, c1 = -1, c2 = -1;
    for (int i = 0; i < n02; i++) {
        if (s[SA12[i]] != c0 || s[SA12[i] + 1] != c1 || s[SA12[i] + 2] != c2) {
            name++;
            c0 = s[SA12[i]];
            c1 = s[SA12[i] + 1];
            c2 = s[SA12[i] + 2];
        }
        int half = SA12[i] % 3 == 1 ? 0 : n0;
        s12[SA12[i] / 3 + half] = name;
    }
    // recurse if names are not yet unique
    if (name < n02) {
        suffixArray(s12, SA12, n02, name);
        // store unique names in s12 using the suffix array
        for (int i = 0; i < n02; i++) {
            s12[SA12[i]] = i + 1;
        }
    } else { // generate the suffix array of s12 directly
        for (int i = 0; i < n02; i++) {
            SA12[s12[i] - 1] = i;
        }
    }
    // stably sort the mod 0 suffixes from SA12 by their first character
    for (int i = 0, j = 0; i < n02; i++) {
        if (SA12[i] < n0) {
            s0[j++] = 3 * SA12[i];
        }
    }
    radixPass(s0, SA0, s, n0, K);
    // merge sorted SA0 suffixes and sorted SA12 suffixes
    for (int p = 0, t = n0 - n1, k = 0; k < n; k++) {
#define GetI() (SA12[t] < n0 ? SA12[t] * 3 + 1 : (SA12[t] - n0) * 3 + 2)
        int i = GetI();    // pos of current offset 12 suffix
        int j = SA0[p];    // pos of current offset 0 suffix
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
                for (k++; p < n0; p++, k++) {
                    SA[k] = SA0[p];
                }
            }
        } else { // suffix from SA0 is smaller
            SA[k] = j;
            p++;
            if (p == n0) { // done --- only SA12 suffixes left
                for (k++; t < n02; t++, k++) {
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

inline std::vector<int>
calcLcp(const int *s, const int *suf_a, const std::vector<int> &suf_array_index) {
    auto n = static_cast<int>(suf_array_index.size());
    std::vector<int> lcp(n);
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (suf_array_index[i] != n - 1) {
            while (std::max(i + k, suf_a[suf_array_index[i] + 1] + k) < n
                   && s[i + k] == s[suf_a[suf_array_index[i] + 1] + k]) {
                k++;
            }
            lcp[suf_array_index[i]] = k;
            if (k > 0) {
                k--;
            }
        } else {
            lcp[n - 1] = -1; // TODO: erase?
        }
    }
    lcp.pop_back(); // todo: erase?
    return lcp;
}

} // namespace detail

// requires ts to have distinct strings
// if s=(t_i1, t_i2, .. t_ik) returns (i1, i2, .. ik)
// returns {} if greedy fails
inline std::vector<int> wordPiece(const std::string_view text, const std::vector<std::string> &vocab) {
    if (text.empty()) {
        return {};
    }
    int64_t longest_word_vocab = 1;
    int64_t total_length = text.size() + 1;
    for (const auto &t : vocab) {
        total_length += t.size() + 1;
        longest_word_vocab = std::max(longest_word_vocab, std::ssize(t));
    }
    int *S = new int[total_length + 3];
    int alphabet_size = 1;

    {
        // elements of s and ts must be > 1, so for example use c - 'a' + 2
        size_t pos = 0;
        for (char c : text) {
            int alpha = c - 'a' + 2;
            S[pos++] = alpha;
            alphabet_size = std::max(alphabet_size, alpha);
        }
        S[pos++] = 1;
        for (const std::string_view word : vocab) {
            for (char c : word) {
                int alpha = c - 'a' + 2;
                S[pos++] = alpha;
                alphabet_size = std::max(alphabet_size, alpha);
            }
            S[pos++] = 1;
        }
        assert(pos == total_length);
    }

    S[total_length] = S[total_length + 1] = S[total_length + 2] = 0;
    // int64_t before = currentTs();
    int *suf = new int[total_length + 3];
    suf_array3n::suffixArray(S, suf, total_length, alphabet_size);
    // std::cout << "sufarray built in " << currentTs() - before << '\n';

    std::vector<int> suf_array_index(total_length);
    for (int i = 0; i < total_length; i++) {
        suf_array_index[suf[i]] = i;
    }

    std::vector<int> lcp = calcLcp(S, suf, suf_array_index);
    delete[] S;
    delete[] suf;

    std::vector<int> who(total_length, -1);

    int vocab_start_pos = static_cast<int>(text.size()) + 1;
    for (int i = 0; i < static_cast<int64_t>(vocab.size()); i++) {
        who[suf_array_index[vocab_start_pos]] = i;
        vocab_start_pos += vocab[i].size() + 1;
    }
    auto get_closest = [&lcp, &who, total_length, longest_word_vocab, &vocab]() {
        std::vector<int> result(total_length, -1);
        // (i, |i|); i is index in ts
        std::vector<std::pair<int, int>> st;
        st.reserve(longest_word_vocab);
        for (int i = 0; i < total_length; i++) {
            if (i > 0) {
                while (!st.empty() && st.back().second > lcp[i - 1]) {
                    st.pop_back();
                }
            }
            if (who[i] != -1) {
                std::pair<int, int> val = {who[i], vocab[who[i]].size()};
                assert(st.empty() || st.back().second < val.second);
                st.emplace_back(val);
            }
            if (!st.empty()) {
                result[i] = st.back().first;
            }
        }
        return result;
    };

    std::vector<int> L = get_closest();
    std::reverse(who.begin(), who.end());
    std::reverse(lcp.begin(), lcp.end());
    std::vector<int> R = get_closest();

    std::vector<int> answer;
    answer.reserve(text.size() / ((total_length - text.size()) / vocab.size()));

    int64_t match_index = 0;
    while (match_index < static_cast<int64_t>(text.size())) {
        int id = suf_array_index[match_index];
        int x = L[id];
        int y = R[total_length - 1 - id];
        int matched_word = -1;

        if (x != -1 && y != -1) {
            matched_word = vocab[x].size() > vocab[y].size() ? x : y;
        } else if (x != -1 || y != -1) {
            matched_word = std::max(x, y);
        } else {
            return {};
        }

        answer.push_back(matched_word);
        match_index += vocab[matched_word].size();
    }

    assert(match_index == static_cast<int64_t>(text.size()));
    return answer;
}

} // namespace word_piece

#endif // WORD_PIECE_H
