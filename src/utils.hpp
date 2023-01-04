// Copyright (c) 2023 Gleb Koveshnikov

#pragma once

#include <string>
#include <vector>

#include "third_party/thread_pool.hpp"

namespace detail {

int64_t currentTs();

std::vector<uint32_t> parseText(const std::string &text, ThreadPool &thread_pool);

std::vector<uint32_t> readTextFromFile(const std::string &filepath, ThreadPool &thread_pool);

std::vector<std::vector<uint32_t>> parseVocab(const std::vector<std::string> &vocab);

std::vector<std::vector<uint32_t>> readVocabFromFile(const std::string &filepath);

} // namespace detail
