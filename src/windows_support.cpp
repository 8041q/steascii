#include "windows_support.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <system_error>

namespace steascii {
namespace {

[[noreturn]] void fail_windows(const char* operation) {
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
}

class FileHandle {
public:
    explicit FileHandle(HANDLE value) : value_(value) {}
    ~FileHandle() { if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    HANDLE get() const { return value_; }
private:
    HANDLE value_;
};

struct GlobalMemoryDeleter {
    void operator()(void* memory) const { GlobalFree(memory); }
};

} // namespace

std::wstring windows_text(std::u16string_view text) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t), "Windows requires UTF-16 wchar_t");
    return std::wstring(text.begin(), text.end());
}

std::u16string decode_windows_ansi(std::string_view bytes) {
    if (bytes.empty()) return {};
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("The text is too large to decode.");
    }
    const int length = static_cast<int>(bytes.size());
    const int count = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, bytes.data(), length, nullptr, 0);
    if (!count) {
        throw std::runtime_error("The file cannot be decoded with this PC's Windows ANSI encoding. Please save it as UTF-8.");
    }
    std::wstring decoded(static_cast<std::size_t>(count), L'\0');
    if (!MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, bytes.data(), length, decoded.data(), count)) {
        fail_windows("Could not decode the file");
    }
    return std::u16string(decoded.begin(), decoded.end());
}

std::u16string read_text_file(const std::wstring& path) {
    // Share readers, but exclude concurrent writers while reading the snapshot.
    FileHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE) fail_windows("Could not open the file");
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file.get(), &length)) fail_windows("Could not determine the file size");
    if (length.QuadPart < 0 || static_cast<unsigned long long>(length.QuadPart) > maximum_file_bytes) {
        throw std::runtime_error("This file is too large. Please choose a text file of 32 MiB or less.");
    }
    std::string bytes(static_cast<std::size_t>(length.QuadPart), '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD amount = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, 1024 * 1024));
        DWORD read = 0;
        if (!ReadFile(file.get(), bytes.data() + offset, amount, &read, nullptr)) fail_windows("Could not read the file");
        if (read == 0) throw std::runtime_error("The file ended unexpectedly. Please open it again.");
        offset += read;
    }
    return decode_text(bytes, decode_windows_ansi);
}

void copy_to_clipboard(HWND owner, std::u16string_view text) {
    if (text.find(u'\0') != std::u16string_view::npos) {
        throw std::runtime_error("The result contains null characters and cannot be copied.");
    }
    const auto content = windows_text(text);
    const auto byte_count = (content.size() + 1) * sizeof(wchar_t);
    std::unique_ptr<void, GlobalMemoryDeleter> memory(GlobalAlloc(GMEM_MOVEABLE, byte_count));
    if (!memory) throw std::runtime_error("There is not enough memory to copy the result.");
    void* destination = GlobalLock(memory.get());
    if (!destination) fail_windows("Could not prepare clipboard data");
    CopyMemory(destination, content.c_str(), byte_count);
    GlobalUnlock(memory.get());

    if (!OpenClipboard(owner)) throw std::runtime_error("The clipboard is busy. Please try Copy result again.");
    struct ClipboardCloser { ~ClipboardCloser() { CloseClipboard(); } } closer;
    if (!EmptyClipboard()) fail_windows("Could not clear the clipboard");
    if (!SetClipboardData(CF_UNICODETEXT, memory.get())) fail_windows("Could not copy the result");
    // Windows owns this allocation after SetClipboardData succeeds.
    memory.release();
}

} // namespace steascii
