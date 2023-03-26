// Copyright (c) 2023 Gleb Koveshnikov

#pragma once

#include <string>
#include <vector>

namespace word_piece {

namespace linear {

std::vector<int> encode(const std::string &text, const std::vector<std::string> &vocab);

std::vector<int> encode(const std::string &text_file, const std::string &vocab_file);

void encodeExternal(const std::string &text_file,
                          const std::string &vocab_file,
                          const std::string &out_file,
                          size_t memory_limit);

} // namespace linear

namespace fast {

std::vector<int> encode(const std::string &text, const std::vector<std::string> &vocab);

std::vector<int> encode(const std::string &text_file, const std::string &vocab_file);

std::vector<std::string> decode(const std::string vocab_file, const std::vector<int>& ids);

void encodeExternal(const std::string &text_file,
                        const std::string &vocab_file,
                        const std::string &out_file,
                        size_t memory_limit);

} // namespace fast

} // namespace word_piece
