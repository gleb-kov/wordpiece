// Copyright (c) 2023 Gleb Koveshnikov

#include "word_piece.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "third_party/thread_pool.hpp"
#include "third_party/utf8.hpp"
#include "utils.hpp"

static std::vector<int> fastWordPieceImpl(const std::vector<uint32_t> &text,
                                          const utils::WordPieceVocabulary &vocab) {
    using WordMap = std::unordered_map<vkcom::VectorSegment, int>;
    WordMap prefix_to_id; // no ## in word prefix
    WordMap suffix_to_id; // ## in word prefix

    size_t max_len = 0;
    for (size_t i = 0; i < vocab.tokens.size(); i++) {
        const auto &token = vocab.tokens[i];
        max_len = std::max(max_len, token.word.size());
        vkcom::VectorSegmentBuilder segment(token.word);
        WordMap *word_to_id = token.is_prefix ? &prefix_to_id : &suffix_to_id;
        (*word_to_id)[segment.finish()] = static_cast<int>(i);
    }
    max_len = std::min(max_len, text.size());

    const auto worker = [unk_token_id = vocab.unk_token_id, max_len, &text, &prefix_to_id, &suffix_to_id](size_t begin, size_t end) {
        std::vector<int> token_ids;
        token_ids.reserve((end - begin) / max_len + 1);

        while (begin != end && vkcom::is_space(text[begin])) {
            ++begin;
        }

        while (begin != end) {
            const size_t len = std::min(max_len, end - begin);
            const uint32_t *segment_begin = text.data() + static_cast<int64_t>(begin);
            const uint32_t *segment_end = segment_begin + static_cast<int64_t>(len);
            const uint32_t prev_char = begin == 0 ? vkcom::SPACE_TOKEN : *(segment_begin - 1);
            const bool is_word_prefix = vkcom::is_space(prev_char) || vkcom::is_punctuation(*segment_begin) || vkcom::is_punctuation(prev_char);
            const WordMap *word_to_id = is_word_prefix ? &prefix_to_id : &suffix_to_id;

            vkcom::VectorSegmentBuilder segment(segment_begin, segment_end);
            while (!segment.empty()) {
                auto it = word_to_id->find(segment.finish());
                if (it != word_to_id->end()) {
                    token_ids.push_back(it->second);
                    begin += segment.size();
                    break;
                } else {
                    segment.pop_back();
                }
            }
            if (segment.empty()) {
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
        const size_t thread_count = std::min(utils::globalThreadPool().maxThreads(), text.size() / kWorkBatch);
        const size_t work_batch = text.size() / thread_count + 1;
        std::vector<std::vector<int>> per_thread_token_ids(thread_count);
        size_t work_begin = 0;
        for (size_t thread_id = 0; thread_id < thread_count && work_begin < text.size();
             thread_id++) {
            size_t work_end = std::min(text.size(), work_begin + work_batch);
            while (work_end < text.size() && !vkcom::is_space(text[work_end])) {
                ++work_end;
            }
            utils::globalThreadPool().submit([thread_id, work_begin, work_end, &per_thread_token_ids, &worker] {
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
                std::memcpy(token_ids.data() + work_begin,
                            segment.data(),
                            segment.size() * sizeof(int));
                work_begin += segment.size();
            }
        }
    }

    return token_ids;
}

namespace word_piece {

std::vector<int>
fastWordPiece(const std::string &text, const std::vector<std::string> &vocab) {
    if (text.empty()) {
        return {};
    }
    const std::vector<uint32_t> text_utf8 = utils::parseText(text, utils::globalThreadPool());
    const utils::WordPieceVocabulary vocab_utf8 = utils::parseVocab(vocab);

    return fastWordPieceImpl(text_utf8, vocab_utf8);
}

std::vector<int>
fastWordPiece(const std::string &text_filepath, const std::string &vocab_filepath) {
    const std::vector<uint32_t> text_utf8 = utils::readTextFromFile(text_filepath, utils::globalThreadPool());
    if (text_utf8.empty()) {
        return {};
    }
    const utils::WordPieceVocabulary vocab_utf8 = utils::readVocabFromFile(vocab_filepath);
    return fastWordPieceImpl(text_utf8, vocab_utf8);
}

} // namespace word_piece
