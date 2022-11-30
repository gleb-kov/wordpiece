#ifndef WORDPIECE_H
#define WORDPIECE_H

#include <bits/stdc++.h>

using namespace std;

namespace SufArray3n {
inline bool leq(int a1, int a2, int b1, int b2) { return (a1 < b1 || (a1 == b1 && a2 <= b2)); }

// and triples
inline bool leq(int a1, int a2, int a3, int b1, int b2, int b3) {
    return (a1 < b1 || (a1 == b1 && leq(a2, a3, b2, b3)));
} // and triples

// stably sort a[0..n-1] to b[0..n-1] with keys in 0..K from r
inline void radixPass(int *a, int *b, int *r, int n, int K) { // count occurrences
    int *c = new int[K + 1];                                  // counter array
    for (int i = 0; i <= K; i++)
        c[i] = 0; // reset counters
    for (int i = 0; i < n; i++)
        c[r[a[i]]]++;                     // count occurrences
    for (int i = 0, sum = 0; i <= K; i++) // exclusive prefix sums
    {
        int t = c[i];
        c[i] = sum;
        sum += t;
    }
    for (int i = 0; i < n; i++)
        b[c[r[a[i]]]++] = a[i]; // sort
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
    for (int i = 0, j = 0; i < n + (n0 - n1); i++)
        if (i % 3 != 0)
            s12[j++] = i;
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
        if (SA12[i] % 3 == 1)
            s12[SA12[i] / 3] = name; // left half
        else
            s12[SA12[i] / 3 + n0] = name; // right half
    }
    // recurse if names are not yet unique
    if (name < n02) {
        suffixArray(s12, SA12, n02, name);
        // store unique names in s12 using the suffix array
        for (int i = 0; i < n02; i++)
            s12[SA12[i]] = i + 1;
    } else // generate the suffix array of s12 directly
        for (int i = 0; i < n02; i++)
            SA12[s12[i] - 1] = i;
    // stably sort the mod 0 suffixes from SA12 by their first character
    for (int i = 0, j = 0; i < n02; i++)
        if (SA12[i] < n0)
            s0[j++] = 3 * SA12[i];
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
            if (t == n02) // done --- only SA0 suffixes left
                for (k++; p < n0; p++, k++)
                    SA[k] = SA0[p];
        } else { // suffix from SA0 is smaller
            SA[k] = j;
            p++;
            if (p == n0) // done --- only SA12 suffixes left
                for (k++; t < n02; t++, k++)
                    SA[k] = GetI();
        }
    }
    delete[] s12;
    delete[] SA12;
    delete[] SA0;
    delete[] s0;
}
} // namespace SufArray3n

inline vector<int> calc_lcp(int *suf_a, const int *s, int n) {
    vector<int> obr(n), mas(n);
    for (int i = 0; i < n; i++)
        obr[suf_a[i]] = i;
    int k = 0;
    for (int i = 0; i < n; i++)
        if (obr[i] == n - 1)
            mas[n - 1] = -1;
        else {
            while (max(i + k, suf_a[obr[i] + 1] + k) < n && s[i + k] == s[suf_a[obr[i] + 1] + k])
                k++;
            mas[obr[i]] = k;
            if (k > 0)
                k--;
        }
    mas.pop_back();
    return mas;
}

// requires ts to have distinct strings
// if s=(t_i1, t_i2, .. t_ik) returns (i1, i2, .. ik)
// returns {} if greedy fails
inline vector<int> get(string s, vector<string> ts) {
    if (s.empty() || ts.empty())
        return {};
    // must be distinct
    {
        unordered_set<string> tss{ts.begin(), ts.end()};
        assert(tss.size() == ts.size());
    }
    int total_length = s.size() + 1;
    for (auto &t : ts)
        total_length += t.size() + 1;
    int *S = new int[total_length + 3];
    int *suf = new int[total_length + 3];
    // elements of s and ts must be > 1, so for example use c - 'a' + 2
    int pos = 0;
    for (char c : s)
        S[pos++] = c - 'a' + 2;
    S[pos++] = 1;
    vector<int> ts_start_pos;
    ts_start_pos.reserve(ts.size());
    for (auto &t : ts) {
        ts_start_pos.emplace_back(pos);
        for (char c : t)
            S[pos++] = c - 'a' + 2;
        S[pos++] = 1;
    }
    assert(pos == total_length);
    int AL = 1;
    for (int i = 0; i < pos; i++)
        AL = max(AL, S[i]);
    S[pos] = S[pos + 1] = S[pos + 2] = 0;
    SufArray3n::suffixArray(S, suf, total_length, AL);
    auto lcp = calc_lcp(suf, S, pos);
    vector<int> who(pos, -1);
    vector<int> ob(pos);
    for (int i = 0; i < pos; i++)
        ob[suf[i]] = i;
    for (int i = 0; i < static_cast<int64_t>(ts.size()); i++) {
        who[ob[ts_start_pos[i]]] = i;
    }
    auto get_closest = [&lcp, &who, &pos, &ts]() {
        vector<int> res(pos, -1);
        // (i, |i|); i is index in ts
        vector<pair<int, int>> st;
        for (int i = 0; i < pos; i++) {
            if (i > 0) {
                int val = lcp[i - 1];
                while (!st.empty() && st.back().second > val)
                    st.pop_back();
            }
            if (who[i] != -1) {
                pair<int, int> val = {who[i], ts[who[i]].size()};
                if (!st.empty())
                    assert(st.back().second < val.second);
                st.emplace_back(val);
            }
            if (!st.empty())
                res[i] = st.back().first;
        }
        return res;
    };
    auto L = get_closest();
    reverse(who.begin(), who.end());
    reverse(lcp.begin(), lcp.end());
    auto R = get_closest();
    reverse(R.begin(), R.end());
    reverse(who.begin(), who.end());
    reverse(lcp.begin(), lcp.end());
    vector<int> biggest(s.size(), -1);
    for (int i = 0; i < static_cast<int64_t>(s.size()); i++) {
        int id = ob[i];
        int x = L[id], y = R[id];
        if (x == -1 || y == -1)
            biggest[i] = max(x, y);
        else if (ts[x].size() > ts[y].size())
            biggest[i] = x;
        else
            biggest[i] = y;
    }
    delete[] S;
    delete[] suf;
    vector<int> answer;
    int cur = 0;
    while (cur < static_cast<int64_t>(s.size())) {
        if (biggest[cur] == -1) {
            // stopped at cur with answer, can print if needed
            return {};
        }
        int nxt = biggest[cur];
        cur += ts[nxt].size();
        answer.emplace_back(nxt);
    }
    assert(cur == static_cast<int64_t>(s.size()));
    return answer;
}

#endif // WORDPIECE_H
