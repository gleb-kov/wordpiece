// Copyright (c) 2023 Gleb Koveshnikov

#pragma once

#include <string>
#include <vector>

namespace word_piece {

std::vector<int>
linearWordPiece(const std::string &text, const std::vector<std::string> &vocab);

std::vector<int>
linearWordPiece(const std::string &text_filepath, const std::string &vocab_filepath);

std::vector<int>
fastWordPiece(const std::string &text, const std::vector<std::string> &vocab);

std::vector<int>
fastWordPiece(const std::string &text_filepath, const std::string &vocab_filepath);

} // namespace word_piece
