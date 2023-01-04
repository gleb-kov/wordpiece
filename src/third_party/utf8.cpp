// Copyright (c) 2019 VK.com

#include "utf8.hpp"

#include <iostream>

namespace vkcom {

bool is_space(uint32_t ch) {
    return (ch < 256 && std::isspace(static_cast<int>(ch))) || (ch == SPACE_TOKEN);
}

bool check_byte(char x) { return (static_cast<uint8_t>(x) & 0xc0u) == 0x80u; }

bool check_symbol_start(char x) { return !check_byte(x); };

bool check_codepoint(uint32_t x) { return (x < 0xd800) || (0xdfff < x && x < 0x110000); }

uint64_t utf_length(char ch) {
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

uint32_t chars_to_utf8(const char *begin, int64_t size, uint64_t *utf8_len) {
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

std::vector<uint32_t> decode_utf8(const char *begin, const char *end) {
    std::vector<uint32_t> decoded_text;
    decoded_text.reserve(static_cast<unsigned long>(end - begin) / 4 + 4);
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

std::vector<uint32_t> decode_utf8(const std::string &utf8_text) {
    return decode_utf8(utf8_text.data(), utf8_text.data() + utf8_text.size());
}

} // namespace vkcom
