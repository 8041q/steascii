#include "windows_support.h"

#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t window_class[] = L"SteasciiNativeWindow";
constexpr int open_id = 1001;
constexpr int copy_id = 1002;
constexpr int result_id = 1003;

struct App {
    HWND window = nullptr;
    HWND open_button = nullptr;
    HWND copy_button = nullptr;
    HWND result_box = nullptr;
    HWND status = nullptr;
    HFONT ui_font = nullptr;
    HFONT result_font = nullptr;
    std::u16string result;
    bool has_result = false;

    ~App() {
        if (ui_font) DeleteObject(ui_font);
        if (result_font) DeleteObject(result_font);
    }

    int pixels(int logical) const { return MulDiv(logical, static_cast<int>(GetDpiForWindow(window)), 96); }

    void fonts() {
        const int height = -MulDiv(10, static_cast<int>(GetDpiForWindow(window)), 72);
        HFONT new_ui = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HFONT new_result = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        const HFONT fallback = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        for (HWND control : {open_button, copy_button, status}) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(new_ui ? new_ui : fallback), TRUE);
        }
        SendMessageW(result_box, WM_SETFONT, reinterpret_cast<WPARAM>(new_result ? new_result : fallback), TRUE);
        if (ui_font) DeleteObject(ui_font);
        if (result_font) DeleteObject(result_font);
        ui_font = new_ui;
        result_font = new_result;
    }

    void layout() const {
        RECT client{};
        GetClientRect(window, &client);
        const int margin = pixels(12);
        const int button_width = pixels(132);
        const int button_height = pixels(32);
        const int status_height = pixels(24);
        const int editor_top = margin * 2 + button_height;
        const int status_top = static_cast<int>(client.bottom) - margin - status_height;
        MoveWindow(open_button, margin, margin, button_width, button_height, TRUE);
        MoveWindow(copy_button, margin * 2 + button_width, margin, button_width, button_height, TRUE);
        MoveWindow(result_box, margin, editor_top, std::max(1, static_cast<int>(client.right) - margin * 2),
                   std::max(1, status_top - editor_top - margin), TRUE);
        MoveWindow(status, margin, status_top, std::max(1, static_cast<int>(client.right) - margin * 2), status_height, TRUE);
    }

    bool create_controls(HINSTANCE instance) {
        open_button = CreateWindowExW(0, L"BUTTON", L"Open text file...",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                      0, 0, 0, 0, window, reinterpret_cast<HMENU>(open_id), instance, nullptr);
        copy_button = CreateWindowExW(0, L"BUTTON", L"Copy result",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                      0, 0, 0, 0, window, reinterpret_cast<HMENU>(copy_id), instance, nullptr);
        result_box = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
                                     ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                                     0, 0, 0, 0, window, reinterpret_cast<HMENU>(result_id), instance, nullptr);
        status = CreateWindowExW(0, L"STATIC", L"Open a text file to convert its spaces to em spaces.",
                                 WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                                 0, 0, 0, 0, window, nullptr, instance, nullptr);
        if (!open_button || !copy_button || !result_box || !status) return false;
        // Avoid the default edit control text limit for large ASCII art.
        SendMessageW(result_box, EM_SETLIMITTEXT, static_cast<WPARAM>(steascii::maximum_file_bytes * 2), 0);
        EnableWindow(copy_button, FALSE);
        fonts();
        layout();
        return true;
    }

    void show_error(const std::exception& error) const {
        // Windows system_error messages use the current Windows ANSI code page.
        std::wstring message = L"Could not complete the operation.";
        try { message = steascii::windows_text(steascii::decode_windows_ansi(error.what())); } catch (...) {}
        MessageBoxW(window, message.c_str(), L"Steascii", MB_OK | MB_ICONERROR);
    }

    void open_file() {
        std::vector<wchar_t> path(32768, L'\0');
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window;
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.lpstrFilter = L"Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
        dialog.lpstrTitle = L"Open text file";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
        if (!GetOpenFileNameW(&dialog)) {
            if (CommDlgExtendedError() != 0) {
                MessageBoxW(window, L"The file picker could not be opened. Please try again.", L"Steascii", MB_OK | MB_ICONERROR);
            }
            return; // Cancellation keeps the current result and Copy state.
        }

        try {
            auto converted = steascii::convert_text(steascii::read_text_file(path.data()));
            const auto display = steascii::windows_text(converted);
            if (!SetWindowTextW(result_box, display.c_str()) ||
                static_cast<std::size_t>(GetWindowTextLengthW(result_box)) != display.size()) {
                // Restore the last usable preview if the editor ran out of memory.
                SetWindowTextW(result_box, steascii::windows_text(result).c_str());
                throw std::runtime_error("The result could not be displayed. Please try a smaller file.");
            }
            result = std::move(converted);
            has_result = true;
            EnableWindow(copy_button, TRUE);
            const auto label = result.empty() ? std::wstring(L"The file is empty. Copy result will copy empty text.")
                : std::wstring(L"Converted: ") + (path.data() + dialog.nFileOffset);
            SetWindowTextW(status, label.c_str());
            SendMessageW(result_box, EM_SETSEL, 0, 0);
            SendMessageW(result_box, EM_SCROLLCARET, 0, 0);
        } catch (const std::exception& error) {
            show_error(error);
        }
    }

    void copy() {
        if (!has_result) return;
        try {
            steascii::copy_to_clipboard(window, result);
            SetWindowTextW(status, L"Result copied. Paste it wherever you need it.");
        } catch (const std::exception& error) {
            show_error(error);
        }
    }
};

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);
    // No C++ exception may unwind through Windows' message dispatcher.
    try {
        switch (message) {
        case WM_CREATE:
            return app->create_controls(reinterpret_cast<CREATESTRUCTW*>(lparam)->hInstance) ? 0 : -1;
        case WM_SIZE:
            if (app->result_box && wparam != SIZE_MINIMIZED) app->layout();
            return 0;
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize = {app->pixels(420), app->pixels(280)};
            return 0;
        }
        case WM_DPICHANGED: {
            const auto* suggested = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(window, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOACTIVATE | SWP_NOZORDER);
            app->fonts();
            app->layout();
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(wparam) == BN_CLICKED) {
                if (LOWORD(wparam) == open_id) app->open_file();
                if (LOWORD(wparam) == copy_id) app->copy();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception& error) {
        app->show_error(error);
        if (message == WM_CREATE) return -1;
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSEXW definition{};
    definition.cbSize = sizeof(definition);
    definition.lpfnWndProc = window_proc;
    definition.hInstance = instance;
    definition.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    definition.hIconSm = definition.hIcon;
    definition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    definition.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    definition.lpszClassName = window_class;
    if (!RegisterClassExW(&definition)) {
        MessageBoxW(nullptr, L"Steascii could not initialize its window.", L"Steascii", MB_OK | MB_ICONERROR);
        return 1;
    }
    App app;
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, window_class, L"Steascii", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 860, 600, nullptr, nullptr, instance, &app);
    if (!window) {
        MessageBoxW(nullptr, L"Steascii could not create its window.", L"Steascii", MB_OK | MB_ICONERROR);
        return 1;
    }
    ShowWindow(window, show);
    UpdateWindow(window);
    SetFocus(app.open_button);
    MSG message{};
    BOOL next;
    while ((next = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return next == -1 ? 1 : static_cast<int>(message.wParam);
}
