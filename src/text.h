#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace steascii {

// UTF-16 keeps the portable conversion identical to Windows Unicode controls.
using AnsiDecoder = std::function<std::u16string(std::string_view)>;

// Unmarked input is tried as strict UTF-8 first, then the supplied ANSI decoder.
// BOM-marked Unicode input never falls back to ANSI on a decoding error.
std::u16string decode_text(std::string_view bytes, const AnsiDecoder& ansi_decoder = {});

// Replace U+0020 with U+2003 and normalize CR, LF and CRLF to Windows CRLF.
// Reject nulls and unpaired surrogates, which cannot be safely shown/copied.
std::u16string convert_text(std::u16string_view text);

} // namespace steascii
