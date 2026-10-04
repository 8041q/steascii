#include "text.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int checks = 0;

void expect(bool condition, const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
}

template<class Function>
void rejects(Function function, const char* description) {
    bool rejected = false;
    try { function(); } catch (const std::runtime_error&) { rejected = true; }
    expect(rejected, description);
}

std::string utf16_bytes(std::u16string_view text, bool little_endian) {
    std::string bytes = little_endian ? "\xFF\xFE" : "\xFE\xFF";
    for (char16_t c : text) {
        const auto low = static_cast<char>(c & 0xFF);
        const auto high = static_cast<char>(c >> 8);
        bytes += little_endian ? std::string{low, high} : std::string{high, low};
    }
    return bytes;
}

void run() {
    using steascii::convert_text;
    using steascii::decode_text;
    expect(convert_text(u" a  b ") == u"\u2003a\u2003\u2003b\u2003", "replace repeated, leading and trailing spaces");
    expect(convert_text(u"").empty(), "empty conversion");
    expect(convert_text(u"a\nb\rc\r\nd\n\ne") == u"a\r\nb\r\nc\r\nd\r\n\r\ne", "normalize mixed line endings and blank lines");
    expect(convert_text(u"\t\u250C \u00E9\U0001F642\u00A0\u2003\t") == u"\t\u250C\u2003\u00E9\U0001F642\u00A0\u2003\t", "preserve tabs, art, accents, emoji and existing spaces");
    expect(convert_text(u"no newline") == u"no\u2003newline", "do not add trailing newline");
    expect(convert_text(u"\r\n\r\n") == u"\r\n\r\n", "preserve Windows line breaks");
    expect(decode_text("").empty(), "empty decoding");
    expect(decode_text("ASCII \t\n") == u"ASCII \t\n", "decode ASCII");
    expect(decode_text(u8"\u250C \u00E9\U0001F642") == u"\u250C \u00E9\U0001F642", "decode UTF-8 Unicode");
    expect(decode_text(u8"\u007F\u0080\u07FF\u0800\uD7FF\uE000\uFFFF\U00010000\U0010FFFF") ==
           u"\u007F\u0080\u07FF\u0800\uD7FF\uE000\uFFFF\U00010000\U0010FFFF", "UTF-8 boundaries and maximum Unicode scalar");
    expect(decode_text(u8"a\uFEFFb") == u"a\uFEFFb", "preserve a BOM character inside text");
    expect(decode_text(std::string("\xEF\xBB\xBF") + u8"\u250C art") == u"\u250C art", "strip UTF-8 BOM");
    const auto unicode = u"\u250C \u00E9\U0001F642\r\n";
    expect(decode_text(utf16_bytes(unicode, true)) == unicode, "UTF-16 little endian");
    expect(decode_text(utf16_bytes(unicode, false)) == unicode, "UTF-16 big endian");
    expect(decode_text("\xEF\xBB\xBF").empty(), "empty UTF-8 BOM file");
    expect(decode_text("\xFF\xFE").empty(), "empty UTF-16 BOM file");

    bool ansi_called = false;
    const steascii::AnsiDecoder ansi = [&](std::string_view bytes) {
        ansi_called = true;
        expect(bytes == "caf\xE9", "ANSI fallback receives the original bytes");
        return std::u16string(u"caf\u00E9");
    };
    expect(decode_text("caf\xE9", ansi) == u"caf\u00E9" && ansi_called, "use ANSI for unmarked non-UTF-8 input");
    ansi_called = false;
    expect(decode_text("valid", ansi) == u"valid" && !ansi_called, "prefer UTF-8 over ANSI");
    rejects([&] { decode_text(std::string("\xEF\xBB\xBF") + "caf\xE9", ansi); }, "reject invalid marked UTF-8");
    expect(!ansi_called, "BOM errors never fall back to ANSI");

    for (const std::string bytes : {"\xC0\xAF", "\xE0\x80\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\xE2\x82", "\x80", "\xE2\x28\xA1"}) {
        rejects([&] { decode_text(bytes); }, "reject malformed unmarked UTF-8 without an ANSI decoder");
        rejects([&] { decode_text(std::string("\xEF\xBB\xBF") + bytes, ansi); }, "reject malformed BOM-marked UTF-8");
    }
    rejects([&] { decode_text(std::string("\xFF\xFE\x41", 3)); }, "reject odd UTF-16 length");
    rejects([&] { decode_text(utf16_bytes(std::u16string(1, 0xD800), true)); }, "reject incomplete high surrogate");
    rejects([&] { decode_text(utf16_bytes(std::u16string(1, 0xDC00), false)); }, "reject stray low surrogate");
    rejects([&] { decode_text(utf16_bytes(std::u16string{0xD800, u'A'}, true)); }, "reject broken surrogate pair");
    rejects([&] { decode_text(std::string("a\0b", 3), ansi); }, "reject UTF-8 null");
    rejects([&] { decode_text(utf16_bytes(std::u16string{u'a', 0, u'b'}, true)); }, "reject UTF-16 null");
    rejects([&] { convert_text(std::u16string{u'a', 0, u'b'}); }, "reject null before display");
    rejects([&] { convert_text(std::u16string(1, 0xDC00)); }, "reject invalid UTF-16 before display");
    rejects([&] { decode_text(std::string("\xFF\xFE\0\0", 4)); }, "reject UTF-32 LE");
    rejects([&] { decode_text(std::string("\0\0\xFE\xFF", 4)); }, "reject UTF-32 BE");
    rejects([&] { decode_text("\xFF", [](auto) { return std::u16string(1, 0); }); }, "validate ANSI decoder output");
    const std::u16string large(100000, u' ');
    expect(convert_text(large) == std::u16string(large.size(), u'\u2003'), "convert beyond the default Windows edit limit");
    expect(convert_text(convert_text(unicode)) == convert_text(unicode), "conversion is idempotent");
}
} // namespace

int main() {
    try {
        run();
        std::cout << checks << " text checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
