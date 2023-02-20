// Copyright (c) 2023 Gleb Koveshnikov

#pragma once

#include <string>
#include <vector>

namespace word_piece {

std::vector<int> linear(const std::string &text, const std::vector<std::string> &vocab);

std::vector<int> linear(const std::string &text_file, const std::string &vocab_file);

void linearExternal(const std::string &text_file,
                    const std::string &vocab_file,
                    const std::string &out_file,
                    size_t memory_limit);

std::vector<int> fast(const std::string &text, const std::vector<std::string> &vocab);

std::vector<int> fast(const std::string &text_file, const std::string &vocab_file);

void fastExternal(const std::string &text_file,
                  const std::string &vocab_file,
                  const std::string &out_file,
                  size_t memory_limit);

} // namespace word_piece
