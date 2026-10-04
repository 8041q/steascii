#pragma once

#include "text.h"
#include <windows.h>

namespace steascii {

// The editor loads at most 32 MiB of source text to bound memory use.
constexpr std::size_t maximum_file_bytes = 32 * 1024 * 1024;

std::u16string decode_windows_ansi(std::string_view bytes);
std::u16string read_text_file(const std::wstring& path);
void copy_to_clipboard(HWND owner, std::u16string_view text);
std::wstring windows_text(std::u16string_view text);

} // namespace steascii
