#pragma once

#include <algorithm>
#include <cassert>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../thread_pool.hpp"
#include "../utf8.hpp"

namespace detail2 {

inline detail::ThreadPool &globalThreadPool() {
    static detail::ThreadPool pool;
    return pool;
}

} // namespace detail2

inline std::vector<int> naiveTokenization(const std::vector<uint32_t> &text,
                                          const std::vector<std::vector<uint32_t>> &vocab,
                                          int unk_token_id) {
    std::unordered_map<vkcom::VectorSegment, int> word_to_id;
    size_t max_len = 0;
    for (size_t i = 0; i < vocab.size(); i++) {
        assert(!vocab[i].empty());
        assert(word_to_id.count(vocab[i]) == 0);
        vkcom::VectorSegment segment(vocab[i]);
        word_to_id[segment] = static_cast<int>(i);
        max_len = std::max(max_len, vocab[i].size());
    }
    max_len = std::min(max_len, text.size());

    const auto worker = [unk_token_id, max_len, &text, &word_to_id](size_t begin, size_t end) {
        std::vector<int> token_ids;
        token_ids.reserve((end - begin) / max_len + 1);

        while (begin != end && vkcom::is_space(text[begin])) {
            ++begin;
        }

        while (begin != end) {
            const size_t len = std::min(max_len, end - begin);
            std::vector<uint32_t> test{text.begin() + static_cast<int64_t>(begin), text.begin() + static_cast<int64_t>(begin + len)};

            while (!test.empty()) {
                vkcom::VectorSegment segment(test);
                auto it = word_to_id.find(segment);
                if (it != word_to_id.end()) {
                    token_ids.push_back(it->second);
                    begin += test.size();
                    break;
                } else {
                    test.pop_back();
                }
            }
            if (test.empty()) {
                token_ids.push_back(unk_token_id);
                while (begin != end && !vkcom::is_space(text[begin])) {
                    ++begin;
                }
            }
            while (begin != end && vkcom::is_space(text[begin])) {
                ++begin;
            }
        }

        return token_ids;
    };

    static constexpr size_t kWorkBatch = 1'000'000;
    std::vector<int> token_ids;
    if (text.size() < 2 * kWorkBatch) {
        token_ids = worker(0, text.size());
    } else {
        const size_t thread_count = std::min(detail2::globalThreadPool().maxThreads(), text.size() / kWorkBatch);
        const size_t work_batch = text.size() / thread_count + 1;
        std::vector<std::vector<int>> per_thread_token_ids(thread_count);
        size_t work_begin = 0;
        for (size_t thread_id = 0; thread_id < thread_count && work_begin < text.size(); thread_id++) {
            size_t work_end = std::min(text.size(), work_begin + work_batch);
            while (work_end < text.size() && !vkcom::is_space(text[work_end])) {
                ++work_end;
            }
            detail2::globalThreadPool().submit([thread_id, work_begin, work_end, &per_thread_token_ids, &worker]{
                per_thread_token_ids[thread_id] = worker(work_begin, work_end);
            });
            work_begin = work_end;
        }

        detail2::globalThreadPool().waitCompletion();

        size_t token_count = 0;
        for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
            token_count += per_thread_token_ids[thread_id].size();
        }
        token_ids.resize(token_count);
        work_begin = 0;
        for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
            std::vector<int> &segment = per_thread_token_ids[thread_id];
            std::memcpy(token_ids.data() + work_begin, segment.data(), segment.size() * sizeof(int));
            work_begin += segment.size();
        }
    }

    return token_ids;
}

inline std::vector<int> naiveTokenization(const std::string &text, const std::vector<std::string>& vocab,
                                          int unk_token_id = -1) {
    std::vector<uint32_t> text_utf8 = vkcom::decode_utf8(text);
    std::vector<std::vector<uint32_t>> vocab_utf8(vocab.size());
    for (size_t i = 0; i < vocab.size(); i++) {
        vocab_utf8[i] = vkcom::decode_utf8(vocab[i]);
    }
    return naiveTokenization(text_utf8, vocab_utf8, unk_token_id);
}
