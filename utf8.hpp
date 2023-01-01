#pragma once

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace vkcom {

constexpr uint32_t INVALID_UNICODE = 0x0fffffff;

constexpr uint32_t SPACE_TOKEN = 9601;

bool is_space(uint32_t ch) {
    return (ch < 256 && std::isspace(static_cast<int>(ch))) || (ch == SPACE_TOKEN);
}

inline bool check_byte(char x) { return (static_cast<uint8_t>(x) & 0xc0u) == 0x80u; }

inline bool check_codepoint(uint32_t x) { return (x < 0xd800) || (0xdfff < x && x < 0x110000); }

inline uint64_t utf_length(char ch) {
    if ((static_cast<uint8_t>(ch) & 0x80u) == 0) {
        return 1;
    }
    if ((static_cast<uint8_t>(ch) & 0xe0u) == 0xc0) {
        return 2;
    }
    if ((static_cast<uint8_t>(ch) & 0xf0u) == 0xe0) {
        return 3;
    }
    if ((static_cast<uint8_t>(ch) & 0xf8u) == 0xf0) {
        return 4;
    }
    // Invalid utf-8
    return 0;
}

inline uint32_t chars_to_utf8(const char *begin, int64_t size, uint64_t *utf8_len) {
    uint64_t length = utf_length(begin[0]);
    if (length == 1) {
        *utf8_len = 1;
        return static_cast<uint8_t>(begin[0]);
    }
    uint32_t code_point = 0;
    if (size >= 2 && length == 2 && check_byte(begin[1])) {
        code_point += (static_cast<uint8_t>(begin[0]) & 0x1fu) << 6u;
        code_point += (static_cast<uint8_t>(begin[1]) & 0x3fu);
        if (code_point >= 0x0080 && check_codepoint(code_point)) {
            *utf8_len = 2;
            return code_point;
        }
    } else if (size >= 3 && length == 3 && check_byte(begin[1]) && check_byte(begin[2])) {
        code_point += (static_cast<uint8_t>(begin[0]) & 0x0fu) << 12u;
        code_point += (static_cast<uint8_t>(begin[1]) & 0x3fu) << 6u;
        code_point += (static_cast<uint8_t>(begin[2]) & 0x3fu);
        if (code_point >= 0x0800 && check_codepoint(code_point)) {
            *utf8_len = 3;
            return code_point;
        }
    } else if (size >= 4 && length == 4 && check_byte(begin[1]) && check_byte(begin[2])
               && check_byte(begin[3])) {
        code_point += (static_cast<uint8_t>(begin[0]) & 0x07u) << 18u;
        code_point += (static_cast<uint8_t>(begin[1]) & 0x3fu) << 12u;
        code_point += (static_cast<uint8_t>(begin[2]) & 0x3fu) << 6u;
        code_point += (static_cast<uint8_t>(begin[3]) & 0x3fu);
        if (code_point >= 0x10000 && check_codepoint(code_point)) {
            *utf8_len = 4;
            return code_point;
        }
    }
    // Invalid utf-8
    *utf8_len = 1;
    return INVALID_UNICODE;
}

inline std::vector<uint32_t> decode_utf8(const char *begin, const char *end) {
    std::vector<uint32_t> decoded_text;
    uint64_t utf8_len = 0;
    bool invalid_input = false;
    for (; begin < end; begin += utf8_len) {
        uint32_t code_point = chars_to_utf8(begin, end - begin, &utf8_len);
        if (code_point != INVALID_UNICODE) {
            decoded_text.push_back(code_point);
        } else {
            invalid_input = true;
        }
    }
    if (invalid_input) {
        std::cerr << "WARNING Input contains invalid unicode characters." << std::endl;
    }
    return decoded_text;
}

inline std::vector<uint32_t> decode_utf8(const std::string &utf8_text) {
    return decode_utf8(utf8_text.data(), utf8_text.data() + utf8_text.size());
}

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
        for (auto it = begin_; it != end_; it++) {
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
