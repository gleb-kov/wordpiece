// Copyright (c) 2023 Gleb Koveshnikov

#pragma once

#include <algorithm>
#include <vector>

#include "utils.hpp"

namespace lcp {

template <typename Count>
static void calcLcpImpl(const uint32_t *str,
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
static std::vector<Count>
calcLcp(const uint32_t *str, const Count *suf_a, const std::vector<Count> &suf_array_index) {
    static constexpr size_t kWorkBatch = 1'000'000;
    const size_t total_length = suf_array_index.size();

    std::vector<Count> lcp(total_length - 1);

    if (total_length < 2 * kWorkBatch) {
        calcLcpImpl(str, suf_a, suf_array_index, lcp, 0, total_length);
    } else {
        const size_t thread_count
            = std::min(detail::globalThreadPool().maxThreads(), total_length / kWorkBatch);
        const size_t work_batch = total_length / thread_count + 1;
        size_t work_start = 0;
        for (size_t i = 0; i < thread_count; i++) {
            size_t work_end = std::min(total_length, work_start + work_batch);
            detail::globalThreadPool().submit(
                [str, suf_a, work_start, work_end, &suf_array_index, &lcp] {
                    calcLcpImpl(str, suf_a, suf_array_index, lcp, work_start, work_end);
                });
            work_start = work_end;
        }
    }

    detail::globalThreadPool().waitCompletion();

    return lcp;
}

} // namespace lcp
