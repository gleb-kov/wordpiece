#ifndef WORD_PIECE_H
#define WORD_PIECE_H

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <mutex>
#include <numeric>
#include <queue>
#include <string>
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

class ThreadPool {
public:
    using Task = std::function<void()>;

public:
    ThreadPool() {
        size_t thread_count = static_cast<size_t>(std::thread::hardware_concurrency());
        if (thread_count == 0) {
            thread_count = 8;
        }
        for (size_t thread = 0; thread < thread_count; ++thread) {
            threads_.emplace_back([this] {
                while (!stop_.load(std::memory_order_relaxed)) {
                    std::unique_lock<std::mutex> lock(mutex_);
                    work_cv_.wait(lock, [this] {
                        return stop_.load(std::memory_order_relaxed) || !task_queue_.empty();
                    });
                    if (stop_.load(std::memory_order_relaxed)) {
                        break;
                    }
                    if (task_queue_.empty()) {
                        continue;
                    }
                    ++active_tasks_;
                    auto task = std::move(task_queue_.front());
                    task_queue_.pop();
                    lock.unlock();
                    task();
                    lock.lock();
                    --active_tasks_;
                    complete_cv_.notify_one();
                }
            });
        }
    }

    ~ThreadPool() {
        stop_.store(true, std::memory_order_relaxed);
        work_cv_.notify_all();
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void submit(Task&& task) {
        {
            std::lock_guard<std::mutex> lg(mutex_);
            task_queue_.emplace(std::move(task));
        }
        work_cv_.notify_one();
    }

    void waitCompletion() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (active_tasks_ != 0 || !task_queue_.empty()) {
            complete_cv_.wait(lock, [this]{
                return active_tasks_ == 0 && task_queue_.empty();
            });
        }
    }

    [[nodiscard]] size_t maxThreads() const noexcept {
        return threads_.size();
    }

private:
    std::atomic<bool> stop_{false};
    size_t active_tasks_{0};
    std::mutex mutex_;
    std::condition_variable work_cv_;
    std::condition_variable complete_cv_;
    std::vector<std::thread> threads_;
    std::queue<Task> task_queue_;
};

inline ThreadPool &globalThreadPool() {
    static ThreadPool pool;
    return pool;
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
    explicit RadixPassSolver(size_t alphabet_size) : common_count_(alphabet_size + 1) {
        size_t thread_count = static_cast<size_t>(std::thread::hardware_concurrency());
        if (thread_count == 0) {
            thread_count = 8;
        }

        // TODO: check if revese index order is faster
        per_thread_count_.assign(thread_count, std::vector<Count>(alphabet_size + 1));

        threads_.reserve(thread_count);
        for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
            thread_work_.push_back(Work{false, 0, 0});

            threads_.emplace_back([this, thread_id] {
                while (true) {
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        work_cv_.wait(lock, [this, thread_id]{
                            return thread_work_[thread_id].active;
                        });
                    }
                    auto &work = thread_work_[thread_id];
                    fillCount(thread_id, work.begin, work.end);

                    {
                        std::lock_guard lock(mutex_);
                        work.active = false;
                        size_t active = active_thread_count_.fetch_sub(1);
                        if (active == 1) {
                            main_cv_.notify_one();
                        }
                    }
                }
            });
        }
    }

    // stably sort a[0..n-1] to b[0..n-1] with keys in 0..alphabet_size from r
    void sort(Count *a, Count *b, Char *r, size_t n, size_t alphabet_size) {
        if (common_count_.size() < alphabet_size + 1) {
            for (size_t i = 0; i < per_thread_count_.size(); i++){
                per_thread_count_[i].assign(alphabet_size + 1, 0);
            }
            common_count_.assign(alphabet_size + 1, 0);
        } else {
            for (size_t i = 0; i < per_thread_count_.size(); i++) {
                std::memset(per_thread_count_[i].data(), 0, (alphabet_size + 1) * sizeof(Count));
            }
            std::memset(common_count_.data(), 0, (alphabet_size + 1) * sizeof(Count));
        }

        work_a_ = a;
        work_b_ = b;
        work_r_ = r;

        if (n < 2 * kWorkBatch) {
            fillCount(0, 0, n);
            makePrefixSum<true>(alphabet_size);
            sortImpl(0, n);
        } else {
            const size_t thread_cnt = std::min(threads_.size(), n / kWorkBatch);
            const size_t work_batch = n / thread_cnt + 1;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                size_t work_start = 0;
                for (size_t i = 0; i < thread_cnt; i++) {
                    thread_work_[i].begin = work_start;
                    work_start = std::min(n, work_start + work_batch);
                    thread_work_[i].end = work_start;
                    thread_work_[i].active = true;
                }
                active_thread_count_ = thread_cnt;
                work_cv_.notify_all();
                if (active_thread_count_ != 0) {
                    main_cv_.wait(lock, [this] {
                        return active_thread_count_ == 0;
                    });
                }
            }
            makePrefixSum<false>(alphabet_size);
            // auto t2 = detail::currentTs();
            // TODO: parallel
            sortImpl(0, n);
            // auto t3 = detail::currentTs();
            // std::cout << "si " << n << ' ' << t3 - t2 << std::endl;
        }
    }

    ~RadixPassSolver() {
        for (std::thread &t : threads_) {
            t.join();
        }
    }

private:
    void fillCount(size_t thread_id, size_t begin, size_t end) {
        auto &count = per_thread_count_[thread_id];
        for (size_t i = begin; i < end; i++) {
            count[work_r_[work_a_[i]]]++;
        }
    }

    template <bool kSingleThread>
    void makePrefixSum(size_t alphabet_size) {
        Count sum = 0;
        for (size_t i = 0; i <= alphabet_size; i++) {
            Count item_count = 0;
            if constexpr (kSingleThread) {
                item_count = per_thread_count_[0][i];
            } else {
                for (size_t j = 0; j < per_thread_count_.size(); j++) {
                    item_count += per_thread_count_[j][i];
                }
            }
            common_count_[i] = sum;
            sum += item_count;
        }
    }

    void sortImpl(size_t begin, size_t end) {
        for (size_t i = begin; i < end; i++) {
            work_b_[common_count_[work_r_[work_a_[i]]]++] = work_a_[i];
        }
    }

private:
    static constexpr size_t kWorkBatch = 500'000;

    struct Work {
        bool active;
        size_t begin;
        size_t end;
    };

    std::vector<Count> common_count_;
    std::vector<std::vector<Count>> per_thread_count_;

    // under mutex
    std::atomic<size_t> active_thread_count_{0};
    std::vector<Work> thread_work_;
    Count *work_a_;
    Count *work_b_;
    Char *work_r_;

    std::vector<std::thread> threads_;
    std::mutex mutex_;
    std::condition_variable main_cv_;
    std::condition_variable work_cv_;
};

template <typename Char, typename Count>
inline void radixPass(Count *a, Count *b, Char *r, size_t n, size_t alphabet_size) {
    static RadixPassSolver<Char, Count> solver(alphabet_size);
    solver.sort(a, b, r, n, alphabet_size);
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
inline void calcLcpImpl(const uint32_t *str,
                        const Count *suf_a,
                        const std::vector<Count> &suf_array_index,
                        std::vector<Count> &lcp,
                        size_t begin,
                        size_t end) {
    size_t prefix_len = 0;
    for (size_t i = begin; i < end; i++) {
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
}

template <typename Count>
inline std::vector<Count>
calcLcp(const uint32_t *str, const Count *suf_a, const std::vector<Count> &suf_array_index) {
    static constexpr size_t kWorkBatch = 1'000'000;
    const size_t total_length = suf_array_index.size();

    std::vector<Count> lcp(total_length - 1);

    if (total_length < 2 * kWorkBatch) {
        calcLcpImpl(str, suf_a, suf_array_index, lcp, 0, total_length);
    } else {
        const size_t thread_cnt = std::min(detail::globalThreadPool().maxThreads(), total_length / kWorkBatch);
        const size_t work_batch = total_length / thread_cnt + 1;
        size_t work_start = 0;
        for (size_t i = 0; i < thread_cnt; i++) {
            size_t work_end = std::min(total_length, work_start + work_batch);
            detail::globalThreadPool().submit([str, suf_a, &suf_array_index, &lcp, work_start, work_end]{
                calcLcpImpl(str, suf_a, suf_array_index, lcp, work_start, work_end);
            });
            work_start = work_end;
        }
    }

    detail::globalThreadPool().waitCompletion();

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

    auto t1 = detail::currentTs();
    // TODO: parallel
    std::vector<Count> lcp = detail::calcLcp<Count>(S, suf, suf_array_index);
    auto t2 = detail::currentTs();
    std::cout << "lcp " << t2 - t1 << std::endl;
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

    // auto t1 = detail::currentTs();
    // TODO: parallel
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
    // auto t2 = detail::currentTs();
    // std::cout << "matching " << t2 - t1 << std::endl;

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
