#include "text.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace steascii {
namespace {

bool high_surrogate(char16_t c) { return c >= 0xD800 && c <= 0xDBFF; }
bool low_surrogate(char16_t c) { return c >= 0xDC00 && c <= 0xDFFF; }

void validate(std::u16string_view text) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == 0) {
            throw std::runtime_error("The file contains null characters. Please choose a text file.");
        }
        if (high_surrogate(text[i])) {
            if (i + 1 == text.size() || !low_surrogate(text[i + 1])) {
                throw std::runtime_error("The file contains malformed UTF-16 text.");
            }
            ++i;
        } else if (low_surrogate(text[i])) {
            throw std::runtime_error("The file contains malformed UTF-16 text.");
        }
    }
}

std::optional<std::u16string> decode_utf8(std::string_view bytes) {
    std::u16string result;
    result.reserve(bytes.size());
    for (std::size_t i = 0; i < bytes.size();) {
        const auto first = static_cast<unsigned char>(bytes[i++]);
        std::uint32_t point = 0;
        unsigned following = 0;
        std::uint32_t minimum = 0;
        if (first < 0x80) {
            point = first;
        } else if (first >= 0xC2 && first <= 0xDF) {
            point = first & 0x1F;
            following = 1;
            minimum = 0x80;
        } else if (first >= 0xE0 && first <= 0xEF) {
            point = first & 0x0F;
            following = 2;
            minimum = 0x800;
        } else if (first >= 0xF0 && first <= 0xF4) {
            point = first & 0x07;
            following = 3;
            minimum = 0x10000;
        } else {
            return std::nullopt;
        }
        if (bytes.size() - i < following) {
            return std::nullopt;
        }
        for (unsigned n = 0; n < following; ++n) {
            const auto next = static_cast<unsigned char>(bytes[i++]);
            if ((next & 0xC0) != 0x80) {
                return std::nullopt;
            }
            point = (point << 6) | (next & 0x3F);
        }
        if (point < minimum || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF)) {
            return std::nullopt;
        }
        if (point <= 0xFFFF) {
            result.push_back(static_cast<char16_t>(point));
        } else {
            point -= 0x10000;
            result.push_back(static_cast<char16_t>(0xD800 + (point >> 10)));
            result.push_back(static_cast<char16_t>(0xDC00 + (point & 0x3FF)));
        }
    }
    return result;
}

bool starts_with(std::string_view bytes, std::string_view prefix) {
    return bytes.size() >= prefix.size() && bytes.substr(0, prefix.size()) == prefix;
}

std::u16string decode_utf16(std::string_view bytes, bool little_endian) {
    if (bytes.size() % 2 != 0) {
        throw std::runtime_error("The file contains malformed UTF-16 text (incomplete character).");
    }
    std::u16string result;
    result.reserve(bytes.size() / 2);
    for (std::size_t i = 0; i < bytes.size(); i += 2) {
        const auto a = static_cast<unsigned char>(bytes[i]);
        const auto b = static_cast<unsigned char>(bytes[i + 1]);
        result.push_back(static_cast<char16_t>(little_endian ? (b << 8) | a : (a << 8) | b));
    }
    return result;
}

} // namespace

std::u16string decode_text(std::string_view bytes, const AnsiDecoder& ansi_decoder) {
    std::u16string result;
    if (starts_with(bytes, std::string_view("\xFF\xFE\x00\x00", 4)) ||
        starts_with(bytes, std::string_view("\x00\x00\xFE\xFF", 4))) {
        throw std::runtime_error("UTF-32 files are not supported. Please save the file as UTF-8 or UTF-16.");
    }
    if (starts_with(bytes, "\xEF\xBB\xBF")) {
        auto decoded = decode_utf8(bytes.substr(3));
        if (!decoded) {
            throw std::runtime_error("The file has a UTF-8 marker but contains malformed UTF-8 text.");
        }
        result = std::move(*decoded);
    } else if (starts_with(bytes, "\xFF\xFE")) {
        result = decode_utf16(bytes.substr(2), true);
    } else if (starts_with(bytes, "\xFE\xFF")) {
        result = decode_utf16(bytes.substr(2), false);
    } else if (auto decoded = decode_utf8(bytes)) {
        result = std::move(*decoded);
    } else if (ansi_decoder) {
        result = ansi_decoder(bytes);
    } else {
        throw std::runtime_error("The file is not valid UTF-8. Please save it as UTF-8 or UTF-16.");
    }
    validate(result);
    return result;
}

std::u16string convert_text(std::u16string_view text) {
    validate(text);
    std::u16string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == u' ') {
            result.push_back(u'\u2003');
        } else if (text[i] == u'\r' || text[i] == u'\n') {
            if (text[i] == u'\r' && i + 1 < text.size() && text[i + 1] == u'\n') {
                ++i;
            }
            result += u"\r\n";
        } else {
            result.push_back(text[i]);
        }
    }
    return result;
}

} // namespace steascii
