#include "windows_support.h"

#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
int checks = 0;

void expect(bool condition, const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
}

template<class Function>
void rejects(Function function, const char* description) {
    bool rejected = false;
    try { function(); } catch (const std::exception&) { rejected = true; }
    expect(rejected, description);
}

struct TemporaryFile {
    std::filesystem::path path;
    TemporaryFile() {
        wchar_t directory[MAX_PATH + 1]{};
        wchar_t filename[MAX_PATH + 1]{};
        const DWORD length = GetTempPathW(MAX_PATH + 1, directory);
        if (!length || length > MAX_PATH || !GetTempFileNameW(directory, L"sta", 0, filename)) {
            throw std::runtime_error("Could not create a temporary test file");
        }
        DeleteFileW(filename);
        path = std::wstring(filename) + L"-\u250C-\u00E9.txt";
    }
    ~TemporaryFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    void write(std::string_view bytes) const {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!file) throw std::runtime_error("Could not write test file");
    }
};

struct Window {
    HWND handle = CreateWindowExW(0, L"STATIC", L"Steascii clipboard tests", WS_OVERLAPPED,
                                   0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Window() { if (!handle) throw std::runtime_error("Could not create clipboard test owner"); }
    ~Window() { DestroyWindow(handle); }
};

std::u16string clipboard_text(HWND owner) {
    if (!OpenClipboard(owner)) throw std::runtime_error("Could not inspect clipboard");
    struct Close { ~Close() { CloseClipboard(); } } close;
    const HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (!data) throw std::runtime_error("Clipboard has no Unicode text");
    const auto* text = static_cast<const wchar_t*>(GlobalLock(data));
    if (!text) throw std::runtime_error("Could not lock clipboard data");
    struct Unlock { HANDLE data; ~Unlock() { GlobalUnlock(data); } } unlock{data};
    const std::wstring content(text);
    return std::u16string(content.begin(), content.end());
}

void run() {
    TemporaryFile file;
    const auto read = [&] { return steascii::read_text_file(file.path.wstring()); };
    rejects(read, "missing files show an error");
    file.write(u8"\u250C art\n\u00E9 \U0001F642");
    expect(read() == u"\u250C art\n\u00E9 \U0001F642", "read UTF-8 from Unicode filename");
    expect(steascii::convert_text(read()) == u"\u250C\u2003art\r\n\u00E9\u2003\U0001F642", "convert decoded file");
    expect(read() == u"\u250C art\n\u00E9 \U0001F642", "conversion leaves source unchanged");
    file.write("second file");
    expect(steascii::convert_text(read()) == u"second\u2003file", "loading again replaces the prior content");
    file.write("");
    expect(read().empty(), "read empty file");
    file.write(std::string("\xEF\xBB\xBF") + u8"\u250C");
    expect(read() == u"\u250C", "read UTF-8 BOM");
    file.write(std::string("\xFF\xFE\x41\0\x20\0\x3D\xD8\x42\xDE", 10));
    expect(read() == u"A \U0001F642", "read UTF-16 LE");
    file.write(std::string("\xFE\xFF\0\x41\0\x20\xD8\x3D\xDE\x42", 10));
    expect(read() == u"A \U0001F642", "read UTF-16 BE");
    file.write("\xEF\xBB\xBF\xFF");
    rejects(read, "bad marked UTF-8 does not fall back to ANSI");
    file.write(std::string("\xFF\xFE\x41", 3));
    rejects(read, "bad marked UTF-16 fails");
    file.write(std::string("text\0tail", 9));
    rejects(read, "embedded null file fails");
    file.write("caf\xE9");
    wchar_t expected[8]{};
    const int ansi_length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, "caf\xE9", 4, expected, 8);
    if (ansi_length) {
        expect(read() == std::u16string(expected, expected + ansi_length), "legacy ANSI matches this PC's active code page");
    } else {
        rejects(read, "invalid bytes on a UTF-8-configured Windows system are rejected");
    }
    file.write("a");
    std::filesystem::resize_file(file.path, steascii::maximum_file_bytes + 1);
    rejects(read, "oversized files fail before allocating their contents");

    Window owner;
    const auto result = steascii::convert_text(u"\u250C \U0001F642\nnext line");
    steascii::copy_to_clipboard(owner.handle, result);
    expect(clipboard_text(owner.handle) == result, "clipboard round-trips Unicode, em spaces and CRLF");
    const std::u16string large(100000, u'\u2003');
    steascii::copy_to_clipboard(owner.handle, large);
    expect(clipboard_text(owner.handle) == large, "clipboard copies the complete large result");
    steascii::copy_to_clipboard(owner.handle, u"");
    expect(clipboard_text(owner.handle).empty(), "copy empty result");
    rejects([&] { steascii::copy_to_clipboard(owner.handle, std::u16string{u'a', 0, u'b'}); }, "null clipboard content is rejected");

    const HANDLE release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!release) throw std::runtime_error("Could not create clipboard synchronization event");
    std::promise<bool> opened;
    auto ready = opened.get_future();
    std::thread holder([&] {
        const bool locked = OpenClipboard(nullptr) != FALSE;
        opened.set_value(locked);
        if (locked) {
            WaitForSingleObject(release, 5000);
            CloseClipboard();
        }
    });
    const bool locked = ready.get();
    bool busy_reported = false;
    if (locked) {
        try { steascii::copy_to_clipboard(owner.handle, result); }
        catch (const std::runtime_error&) { busy_reported = true; }
    }
    SetEvent(release);
    holder.join();
    CloseHandle(release);
    expect(locked && busy_reported, "busy clipboard reports an error");
    steascii::copy_to_clipboard(owner.handle, result);
    expect(clipboard_text(owner.handle) == result, "clipboard can be retried after a failure");
}
} // namespace

int main() {
    try {
        run();
        std::cout << checks << " Windows checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
