// Copyright (c) 2023 Gleb Koveshnikov

#include "utils.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "third_party/thread_pool.hpp"
#include "third_party/utf8.hpp"

namespace detail {

int64_t currentTs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::vector<uint32_t> parseText(const std::string &text, ThreadPool &thread_pool) {
    static constexpr size_t kWorkBatch = 5'000'000;

    if (text.size() < 2 * kWorkBatch) {
        return vkcom::decode_utf8(text);
    } else {
        const size_t thread_count = std::min(thread_pool.maxThreads(), text.size() / kWorkBatch);
        const size_t work_batch = text.size() / thread_count + 1;
        std::vector<std::vector<uint32_t>> per_thread_text_utf8(thread_count);
        size_t work_start = 0;
        for (size_t thread_id = 0; thread_id < thread_count && work_start < text.size(); thread_id++) {
            size_t work_end = std::min(text.size(), work_start + work_batch);
            while (work_end < text.size() && !vkcom::check_symbol_start(text[work_end])) {
                ++work_end;
            }
            thread_pool.submit(
                [thread_id, work_start, work_end, &per_thread_text_utf8, &text] {
                    const char *begin = text.data() + work_start;
                    const size_t len = work_end - work_start;
                    per_thread_text_utf8[thread_id] = vkcom::decode_utf8(begin, begin + len);
                });
            work_start = work_end;
        }

        thread_pool.waitCompletion();
        size_t text_utf8_size = 0;
        for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
            text_utf8_size += per_thread_text_utf8[thread_id].size();
        }
        std::vector<uint32_t> text_utf8(text_utf8_size);
        text_utf8.resize(text_utf8_size);
        work_start = 0;
        for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
            std::vector<uint32_t> &segment = per_thread_text_utf8[thread_id];
            if (!segment.empty()) {
                std::memcpy(text_utf8.data() + work_start,
                            segment.data(),
                            segment.size() * sizeof(uint32_t));
                work_start += segment.size();
            }
        }
        return text_utf8;
    }
}

std::vector<uint32_t> readTextFromFile(const std::string &filepath, ThreadPool &thread_pool) {
    // TODO: mmap read or read with O_DIRECT
    std::ifstream fin(filepath);
    std::string text = std::string(std::istreambuf_iterator<char>(fin), std::istreambuf_iterator<char>());

    if (text.empty()) {
        return {};
    }

    return parseText(text, thread_pool);
}

std::vector<std::vector<uint32_t>> parseVocab(const std::vector<std::string> &vocab) {
    std::vector<std::vector<uint32_t>> vocab_utf8;
    vocab_utf8.reserve(vocab.size());

    for (const std::string &word : vocab) {
        vocab_utf8.push_back(vkcom::decode_utf8(word));
    }

    return vocab_utf8;
}

std::vector<std::vector<uint32_t>> readVocabFromFile(const std::string &filepath) {
    std::vector<std::vector<uint32_t>> vocab_utf8;
    std::ifstream fin(filepath);
    std::string word;
    while (std::getline(fin, word)) {
        vocab_utf8.push_back(vkcom::decode_utf8(word));
    }

    return vocab_utf8;
}

} // namespace detail
