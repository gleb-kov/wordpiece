// Copyright (c) 2023 Gleb Koveshnikov

#include "word_piece.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/iostreams/device/mapped_file.hpp>

#include "third_party/thread_pool.hpp"
#include "third_party/utf8.hpp"
#include "utils.hpp"

static std::vector<int> encodeFastWordPieceImpl(const std::vector<uint32_t> &text,
                                                const utils::WordPieceVocabulary &vocab) {
  using WordMap = std::unordered_map<vkcom::VectorSegment, int>;
  WordMap prefix_to_id; // no ## in word prefix
  WordMap suffix_to_id; // ## in word prefix

  size_t max_len = 0;
  for (size_t i = 0; i < vocab.tokens.size(); i++) {
    const auto &token = vocab.tokens[i];
    if (token.is_special || token.is_malformed) {
      continue;
    }
    max_len = std::max(max_len, token.word.size());
    vkcom::VectorSegmentBuilder segment(token.word);
    WordMap *word_to_id = token.is_prefix ? &prefix_to_id : &suffix_to_id;
    (*word_to_id)[segment.finish()] = static_cast<int>(i);
  }
  max_len = std::min(max_len, text.size());

  const auto is_word_prefix = [&text](size_t index) {
    return index == 0 || vkcom::is_spacing_char(text[index])
        || vkcom::is_spacing_char(text[index - 1]);
  };

  const auto worker = [&, unk_token_id = vocab.unk_token_id](size_t begin, size_t end) {
    std::vector<int> token_ids;
    token_ids.reserve((end - begin) / max_len + 1);

    while (begin != end && vkcom::is_space(text[begin])) {
      ++begin;
    }

    size_t tokens_since_prefix = 0;

    while (begin != end) {
      size_t word_len = 1;
      if (!vkcom::is_punctuation(text[begin])) {
        while (word_len < std::min(max_len, end - begin)
               && !vkcom::is_spacing_char(text[begin + word_len])) {
          ++word_len;
        }
      }

      const uint32_t *segment_begin = text.data() + static_cast<int64_t>(begin);
      const uint32_t *segment_end = segment_begin + static_cast<int64_t>(word_len);
      const WordMap *word_to_id = is_word_prefix(begin) ? &prefix_to_id : &suffix_to_id;

      vkcom::VectorSegmentBuilder segment(segment_begin, segment_end);
      while (!segment.empty()) {
        auto it = word_to_id->find(segment.finish());
        if (it != word_to_id->end()) {
          ++tokens_since_prefix;
          token_ids.push_back(it->second);
          begin += segment.size();
          break;
        } else {
          segment.pop_back();
        }
      }

      if (segment.empty()) {
        while (tokens_since_prefix > 0) {
          token_ids.pop_back();
          --tokens_since_prefix;
        }
        token_ids.push_back(unk_token_id);
        begin += word_len;
        while (begin != end && !is_word_prefix(begin)) {
          ++begin;
        }
      } else if (begin != end && is_word_prefix(begin)) {
        tokens_since_prefix = 0;
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
    const size_t thread_count
     = std::min(utils::globalThreadPool().maxThreads(), text.size() / kWorkBatch);
    const size_t work_batch = text.size() / thread_count + 1;
    std::vector<std::vector<int>> per_thread_token_ids(thread_count);
    size_t work_begin = 0;
    for (size_t thread_id = 0; thread_id < thread_count && work_begin < text.size(); thread_id++) {
      size_t work_end = std::min(text.size(), work_begin + work_batch);
      while (work_end < text.size() && !vkcom::is_space(text[work_end])) {
        ++work_end;
      }
      utils::globalThreadPool().submit(
       [thread_id, work_begin, work_end, &per_thread_token_ids, &worker] {
         per_thread_token_ids[thread_id] = worker(work_begin, work_end);
       });
      work_begin = work_end;
    }

    utils::globalThreadPool().waitCompletion();

    size_t token_count = 0;
    for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
      token_count += per_thread_token_ids[thread_id].size();
    }
    token_ids.resize(token_count);
    work_begin = 0;
    for (size_t thread_id = 0; thread_id < thread_count; thread_id++) {
      std::vector<int> &segment = per_thread_token_ids[thread_id];
      if (!segment.empty()) {
        std::memcpy(token_ids.data() + work_begin, segment.data(), segment.size() * sizeof(int));
        work_begin += segment.size();
      }
    }
  }

  return token_ids;
}

static std::vector<int>
encodeFastWordPiece(const char *text, size_t size, const utils::WordPieceVocabulary &vocab) {
  if (size == 0) {
    return {};
  }
  const std::vector<uint32_t> text_utf8 = utils::parseText(text, size, utils::globalThreadPool());
  return encodeFastWordPieceImpl(text_utf8, vocab);
}

namespace word_piece::fast {

std::vector<int> encode(const std::string &text, const std::vector<std::string> &vocab) {
  const utils::WordPieceVocabulary vocab_utf8 = utils::parseVocab(vocab);
  return encodeFastWordPiece(text.data(), text.size(), vocab_utf8);
}

std::vector<int> encode(const std::string &text_file, const std::string &vocab_file) {
  const utils::WordPieceVocabulary vocab_utf8 = utils::readVocabFromFile(vocab_file);
  boost::iostreams::mapped_file mmap(text_file, boost::iostreams::mapped_file::readonly);
  return encodeFastWordPiece(mmap.const_data(), mmap.size(), vocab_utf8);
}

std::vector<std::string> decode(const std::string vocab_file, const std::vector<int> &ids) {
  const utils::WordPieceVocabulary vocab_utf8 = utils::readVocabFromFile(vocab_file);
  std::vector<std::string> result;
  result.reserve(ids.size());

  for (int id : ids) {
    if (id < 0 || static_cast<size_t>(id) > vocab_utf8.tokens.size()) {
      std::cerr << "no token " << id << std::endl;
      continue;
    }
    const auto &token = vocab_utf8.tokens.at(static_cast<size_t>(id));
    if (token.is_malformed) {
      std::cerr << "trying to access malformed token" << std::endl;
    } else if (token.is_prefix) {
      result.push_back(vkcom::encode_utf8(token.word));
    } else {
      std::string encoded = "##" + vkcom::encode_utf8(token.word);
      result.push_back(std::move(encoded));
    }
  }

  return result;
}

void encodeExternal(const std::string &text_file,
                    const std::string &vocab_file,
                    const std::string &out_file,
                    size_t memory_limit) {
  const utils::WordPieceVocabulary vocab_utf8 = utils::readVocabFromFile(vocab_file);

  const size_t maxTextBatch = memory_limit / 2;
  boost::iostreams::mapped_file mmap(text_file, boost::iostreams::mapped_file::readonly);
  const char *begin = mmap.const_data();
  size_t size = mmap.size();

  std::ofstream fout(out_file);
  while (size > 0) {
    size_t batch;
    if (size > maxTextBatch) {
      batch = maxTextBatch;
      while (batch < size
             && !vkcom::starts_with_space(begin + batch - 1, static_cast<int64_t>(size - batch))) {
        batch++;
      }
    } else {
      batch = size;
    }

    std::vector<int> ids = encodeFastWordPiece(begin, batch, vocab_utf8);
    for (int id : ids) {
      fout << id << ' ';
    }
    begin += batch;
    size -= batch;
  }
}

} // namespace word_piece::fast
