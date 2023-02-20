// Copyright (c) 2023 Gleb Koveshnikov

#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "src/utils.hpp"
#include "src/word_piece.hpp"

int main(int argc, char *argv[]) {
  if (argc < 4 || argc > 7) {
    throw std::runtime_error("Usage: ./runner <mode> <text_file> <vocab_file> [n_threads] "
                             "[out_file] [memory_limit_mb]. "
                             "Modes: fast, linear, fast-external, linear-external.");
  }

  const std::string mode = argv[1];
  const std::string text_file = argv[2];
  const std::string vocab_file = argv[3];
  const size_t n_threads = argc == 5 ? std::stoull(argv[4]) : 0;
  const std::optional<std::string> out_file = argc >= 6 ? std::optional(argv[5]) : std::nullopt;
  std::optional<size_t> memory_limit
   = argc >= 7 ? std::optional(std::stoull(argv[6])) : std::nullopt;

  if (memory_limit.has_value()) {
    if (*memory_limit < 50) {
      throw std::runtime_error("memory_limit cannot be less than 50Mb");
    }
    *memory_limit *= 1'000'000;
  }

  [[maybe_unused]] auto &thread_pool = utils::globalThreadPool(n_threads);

  if (mode == "fast") {
    std::vector<int> ids = word_piece::fast(text_file, vocab_file);
    std::cout << "Total ids " << ids.size() << std::endl;
    if (out_file) {
      utils::writeToFile(*out_file, ids);
    }
  } else if (mode == "linear") {
    std::vector<int> ids = word_piece::linear(text_file, vocab_file);
    std::cout << "Total ids " << ids.size() << std::endl;
    if (out_file) {
      utils::writeToFile(*out_file, ids);
    }
  } else if (mode == "fast-external") {
    if (!memory_limit.has_value()) {
      throw std::runtime_error("For external mode provide out_file and memory_limit");
    }
    word_piece::fastExternal(text_file, vocab_file, out_file.value(), memory_limit.value());
  } else if (mode == "linear-external") {
    if (!memory_limit.has_value()) {
      throw std::runtime_error("For external mode provide out_file and memory_limit");
    }
    word_piece::linearExternal(text_file, vocab_file, out_file.value(), memory_limit.value());
  } else {
    throw std::runtime_error("Unknown mode");
  }
}
