// Copyright (c) 2019 VK.com

#pragma once

#include <string>
#include <vector>

namespace vkcom {

constexpr uint32_t INVALID_UNICODE = 0x0fffffff;

constexpr uint32_t SPACE_TOKEN = 9601;

bool is_space(uint32_t ch);

bool check_byte(char x);

bool check_symbol_start(char x);

bool check_codepoint(uint32_t x);

uint64_t utf_length(char ch);

uint32_t chars_to_utf8(const char *begin, int64_t size, uint64_t *utf8_len);

std::vector<uint32_t> decode_utf8(const char *begin, const char *end);

std::vector<uint32_t> decode_utf8(const std::string &utf8_text);

struct VectorSegment {
  private:
    constexpr static uint64_t MOD = 2032191299;
    constexpr static uint64_t P = 726328703;

    const uint32_t *begin_;
    const uint32_t *end_;
    uint64_t hash_;

  public:
    VectorSegment(const std::vector<uint32_t> &segment)
        : VectorSegment(segment.data(), segment.data() + segment.size()) {}

    VectorSegment(const uint32_t *begin, const uint32_t *end) : begin_(begin), end_(end) {
        hash_ = 0;
        for (const uint32_t *it = begin_; it != end_; it++) {
            hash_ = (hash_ * P + *it) % MOD;
        }
    }

    bool operator==(const VectorSegment &other) const {
        if (other.hash_ != hash_ || end_ - begin_ != other.end_ - other.begin_) {
            return false;
        }
        for (auto it = begin_, other_it = other.begin_; it != end_; it++, other_it++) {
            if (*it != *other_it) {
                return false;
            }
        }
        return true;
    }

    uint64_t hash() const { return hash_; }
};

} // namespace vkcom

namespace std {

template <>
struct hash<vkcom::VectorSegment> {
    uint64_t operator()(const vkcom::VectorSegment &x) const { return x.hash(); }
};

} // namespace std
