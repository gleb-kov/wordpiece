#include "wordpiece.hpp"

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(0);

    std::string s;
    std::cin >> s;
    int n;
    std::cin >> n;
    std::vector<std::string> ts(n);
    for (auto &i : ts) {
        std::cin >> i;
    }
    auto q = maxMatch(s, ts);
    for (int i : q) {
        std::cout << ts[i] << "|";
    }
    std::cout << '\n';
}
