// http://www.cs.cmu.edu/~guyb/paralg/papers/KarkkainenSanders03.pdf

#pragma once

#include <vector>

namespace saca_dc3 {

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

// stably sort a[0..n-1] to b[0..n-1] with keys in 0..alphabet_size from r
template <typename Char, typename Count>
inline void radixPass(Count *a, Count *b, Char *r, size_t n, size_t alphabet_size) {
    std::vector<Count> count(alphabet_size + 1, 0);
    for (size_t i = 0; i < n; i++) {
        count[r[a[i]]]++;
    }
    Count sum = 0;
    for (size_t i = 0; i <= alphabet_size; i++) {
        Count item_count = count[i];
        count[i] = sum;
        sum += item_count;
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

} // namespace saca_dc3
