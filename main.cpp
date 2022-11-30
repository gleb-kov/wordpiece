#include "wordpiece.hpp"

int main() {
    ios::sync_with_stdio(0);
    cin.tie(0);

    string s;
    cin >> s;
    int n;
    cin >> n;
    vector<string> ts(n);
    for (auto &i : ts)
        cin >> i;
    auto q = get(s, ts);
    for (int i : q)
        cout << ts[i] << "|";
    cout << '\n';
}
