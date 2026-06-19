/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 esp3tek
 */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>

#define WM_TRAYICON (WM_APP + 1)
#define HOTKEY_ID   1
#define TRAY_UID    1
#define IDM_SHOW    1001
#define IDM_EXIT    1002
#define KEY_DELAY_MS 8
#define KEY_HOLD_MS  8
#define WARMUP_MS    150
#define APP_VERSION L"1.2.2"

static const wchar_t CLASS_NAME[]   = L"PasteAsKeystrokesWnd";
static const wchar_t WINDOW_TITLE[] = L"PasteAsKeystrokes";
static const wchar_t MUTEX_NAME[]   = L"PasteAsKeystrokes_SingleInstance_{B7A3}";
static const wchar_t TRAY_TIP[]     = L"PasteAsKeystrokes \u2014 Ctrl+Alt+V";

static BOOL g_tray_added = FALSE;
static HICON g_icon_large = NULL;
static HICON g_icon_small = NULL;

static HICON create_key_icon(int size) {
    HDC screen = GetDC(NULL);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, size, size);
    HBITMAP mask  = CreateBitmap(size, size, 1, 1, NULL);

    HBITMAP old_color = (HBITMAP)SelectObject(mem, color);
    RECT full = { 0, 0, size, size };
    HBRUSH face = CreateSolidBrush(RGB(192, 192, 192));
    FillRect(mem, &full, face);
    DeleteObject(face);

    HPEN white = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN dark  = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
    HPEN black = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

    HPEN old_pen = (HPEN)SelectObject(mem, white);
    MoveToEx(mem, 0, size - 2, NULL); LineTo(mem, 0, 0);
    LineTo(mem, size - 1, 0);

    SelectObject(mem, black);
    MoveToEx(mem, size - 1, 0, NULL); LineTo(mem, size - 1, size - 1);
    LineTo(mem, -1, size - 1);

    if (size >= 24) {
        SelectObject(mem, dark);
        MoveToEx(mem, 1, size - 2, NULL); LineTo(mem, size - 2, size - 2);
        MoveToEx(mem, size - 2, 1, NULL); LineTo(mem, size - 2, size - 1);
    }

    SelectObject(mem, old_pen);
    DeleteObject(white); DeleteObject(dark); DeleteObject(black);

    int font_h = (size * 5) / 8;
    HFONT font = CreateFontW(font_h, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        size >= 24 ? CLEARTYPE_QUALITY : NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Tahoma");
    HFONT old_font = (HFONT)SelectObject(mem, font);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(0, 0, 0));
    DrawTextW(mem, L"V", -1, &full, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem, old_font);
    DeleteObject(font);
    SelectObject(mem, old_color);

    HBITMAP old_mask = (HBITMAP)SelectObject(mem, mask);
    PatBlt(mem, 0, 0, size, size, BLACKNESS);
    SelectObject(mem, old_mask);

    DeleteDC(mem);
    ReleaseDC(NULL, screen);

    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask  = mask;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

static void add_tray_icon(HWND hwnd) {
    if (g_tray_added) return;
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = TRAY_UID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = g_icon_small ? g_icon_small : LoadIconW(NULL, IDI_APPLICATION);
    lstrcpynW(nid.szTip, TRAY_TIP, sizeof(nid.szTip) / sizeof(wchar_t));
    if (Shell_NotifyIconW(NIM_ADD, &nid)) g_tray_added = TRUE;
}

static void remove_tray_icon(HWND hwnd) {
    if (!g_tray_added) return;
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = TRAY_UID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_tray_added = FALSE;
}

static void show_tray_menu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, IDM_SHOW, L"Show");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
}

static void restore_window(HWND hwnd) {
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    remove_tray_icon(hwnd);
}

static void send_key_up(WORD vk) {
    INPUT in = {0};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    in.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));
}

static void release_modifiers(void) {
    static const WORD mods[] = {
        VK_CONTROL, VK_LCONTROL, VK_RCONTROL,
        VK_MENU,    VK_LMENU,    VK_RMENU,
        VK_SHIFT,   VK_LSHIFT,   VK_RSHIFT,
        VK_LWIN,    VK_RWIN
    };
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
        send_key_up(mods[i]);
    }
}

static void send_vk(WORD vk) {
    INPUT in = {0};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    SendInput(1, &in, sizeof(in));
    Sleep(KEY_HOLD_MS);
    in.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));
}

static void send_unicode_char(wchar_t ch) {
    INPUT in[2] = {0};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wScan = ch;
    in[0].ki.dwFlags = KEYEVENTF_UNICODE;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

static void send_char_via_layout(wchar_t ch) {
    SHORT vks = VkKeyScanW(ch);
    BYTE mods = (BYTE)((vks >> 8) & 0xFF);
    if (vks == -1 || (mods & 0xF8)) {
        send_unicode_char(ch);
        return;
    }
    WORD vk = (WORD)(vks & 0xFF);
    BOOL want_shift = mods & 1;
    BOOL want_ctrl  = mods & 2;
    BOOL want_alt   = mods & 4;
    WORD vsc = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    WORD vsc_shift = (WORD)MapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC);
    WORD vsc_ctrl  = (WORD)MapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC);
    WORD vsc_alt   = (WORD)MapVirtualKeyW(VK_MENU, MAPVK_VK_TO_VSC);

    INPUT down[4] = {0};
    int nd = 0;
    if (want_shift) { down[nd].type = INPUT_KEYBOARD; down[nd].ki.wVk = VK_SHIFT;   down[nd].ki.wScan = vsc_shift; nd++; }
    if (want_ctrl)  { down[nd].type = INPUT_KEYBOARD; down[nd].ki.wVk = VK_CONTROL; down[nd].ki.wScan = vsc_ctrl;  nd++; }
    if (want_alt)   { down[nd].type = INPUT_KEYBOARD; down[nd].ki.wVk = VK_MENU;    down[nd].ki.wScan = vsc_alt;   nd++; }
    down[nd].type = INPUT_KEYBOARD; down[nd].ki.wVk = vk; down[nd].ki.wScan = vsc; nd++;
    SendInput((UINT)nd, down, sizeof(INPUT));

    Sleep(KEY_HOLD_MS);

    INPUT up[4] = {0};
    int nu = 0;
    up[nu].type = INPUT_KEYBOARD; up[nu].ki.wVk = vk; up[nu].ki.wScan = vsc; up[nu].ki.dwFlags = KEYEVENTF_KEYUP; nu++;
    if (want_alt)   { up[nu].type = INPUT_KEYBOARD; up[nu].ki.wVk = VK_MENU;    up[nu].ki.wScan = vsc_alt;   up[nu].ki.dwFlags = KEYEVENTF_KEYUP; nu++; }
    if (want_ctrl)  { up[nu].type = INPUT_KEYBOARD; up[nu].ki.wVk = VK_CONTROL; up[nu].ki.wScan = vsc_ctrl;  up[nu].ki.dwFlags = KEYEVENTF_KEYUP; nu++; }
    if (want_shift) { up[nu].type = INPUT_KEYBOARD; up[nu].ki.wVk = VK_SHIFT;   up[nu].ki.wScan = vsc_shift; up[nu].ki.dwFlags = KEYEVENTF_KEYUP; nu++; }
    SendInput((UINT)nu, up, sizeof(INPUT));
}

static void send_surrogate_pair(wchar_t high, wchar_t low) {
    INPUT in[4] = {0};
    for (int i = 0; i < 4; i++) in[i].type = INPUT_KEYBOARD;
    in[0].ki.wScan = high; in[0].ki.dwFlags = KEYEVENTF_UNICODE;
    in[1].ki.wScan = high; in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    in[2].ki.wScan = low;  in[2].ki.dwFlags = KEYEVENTF_UNICODE;
    in[3].ki.wScan = low;  in[3].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
}

static wchar_t *read_clipboard_text(void) {
    if (!OpenClipboard(NULL)) return NULL;
    wchar_t *result = NULL;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        wchar_t *p = (wchar_t *)GlobalLock(h);
        if (p) {
            size_t len = wcslen(p);
            result = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
            if (result) memcpy(result, p, (len + 1) * sizeof(wchar_t));
            GlobalUnlock(h);
        }
    } else {
        h = GetClipboardData(CF_TEXT);
        if (h) {
            char *p = (char *)GlobalLock(h);
            if (p) {
                int wlen = MultiByteToWideChar(CP_ACP, 0, p, -1, NULL, 0);
                if (wlen > 0) {
                    result = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
                    if (result) MultiByteToWideChar(CP_ACP, 0, p, -1, result, wlen);
                }
                GlobalUnlock(h);
            }
        }
    }
    CloseClipboard();
    return result;
}

static void wait_modifiers_released(int max_ms) {
    int elapsed = 0;
    while (elapsed < max_ms) {
        if (!(GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
            !(GetAsyncKeyState(VK_MENU)    & 0x8000) &&
            !(GetAsyncKeyState(VK_SHIFT)   & 0x8000) &&
            !(GetAsyncKeyState(VK_LWIN)    & 0x8000) &&
            !(GetAsyncKeyState(VK_RWIN)    & 0x8000)) return;
        Sleep(10);
        elapsed += 10;
    }
}

static void type_clipboard(void) {
    wait_modifiers_released(500);
    release_modifiers();
    wchar_t *text = read_clipboard_text();
    if (!text || !*text) {
        free(text);
        MessageBeep(MB_ICONWARNING);
        return;
    }

    /* Warm-up before the first keystroke: on remote targets (RDP, Hyper-V
     * VMConnect, KVM) the synthetic modifier KEYUPs above and the first
     * character can arrive so close together that the target still sees
     * Ctrl/Alt as held and eats the first key as a shortcut. This one-time
     * pause lets the target settle before typing begins. */
    Sleep(WARMUP_MS);

    for (wchar_t *p = text; *p; p++) {
        wchar_t c = *p;
        if (c == 0xFEFF && p == text) continue;
        if (c == L'\r') {
            send_vk(VK_RETURN);
            if (*(p + 1) == L'\n') p++;
        } else if (c == L'\n') {
            send_vk(VK_RETURN);
        } else if (c == L'\t') {
            send_unicode_char(L'\t');
        } else if (c >= 0xD800 && c <= 0xDBFF
                   && *(p + 1) >= 0xDC00 && *(p + 1) <= 0xDFFF) {
            send_surrogate_pair(c, *(p + 1));
            p++;
        } else {
            send_char_via_layout(c);
        }
        Sleep(KEY_DELAY_MS);
    }
    free(text);
}

static void draw_key_90s(HDC hdc, int x, int y, int w, int h, const wchar_t *label, HFONT font) {
    RECT r = { x, y, x + w, y + h };
    HBRUSH face = CreateSolidBrush(RGB(192, 192, 192));
    FillRect(hdc, &r, face);
    DeleteObject(face);

    HPEN white = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN dark  = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
    HPEN black = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

    HPEN old_pen = (HPEN)SelectObject(hdc, white);
    MoveToEx(hdc, x, y + h - 2, NULL); LineTo(hdc, x, y);
    LineTo(hdc, x + w - 1, y);

    SelectObject(hdc, black);
    MoveToEx(hdc, x + w - 1, y, NULL); LineTo(hdc, x + w - 1, y + h - 1);
    LineTo(hdc, x - 1, y + h - 1);

    SelectObject(hdc, dark);
    MoveToEx(hdc, x + 1, y + h - 2, NULL); LineTo(hdc, x + w - 2, y + h - 2);
    MoveToEx(hdc, x + w - 2, y + 1, NULL); LineTo(hdc, x + w - 2, y + h - 1);

    SelectObject(hdc, old_pen);
    DeleteObject(white); DeleteObject(dark); DeleteObject(black);

    HFONT old_font = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawTextW(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old_font);
}

static void draw_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    HFONT title_font = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT body_font = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT plus_font = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT key_font = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");

    RECT title_rc = rc;
    title_rc.top = 14;
    title_rc.bottom = title_rc.top + 28;
    HFONT old = (HFONT)SelectObject(hdc, title_font);
    DrawTextW(hdc, L"PasteAsKeystrokes", -1, &title_rc, DT_CENTER | DT_SINGLELINE);

    SelectObject(hdc, body_font);
    RECT body_rc = rc;
    body_rc.top    = 48;
    body_rc.bottom = 102;
    body_rc.left  += 16;
    body_rc.right -= 16;
    DrawTextW(hdc,
        L"Press this combination to type the\n"
        L"clipboard into the focused window:",
        -1, &body_rc, DT_CENTER | DT_WORDBREAK);

    int kw_ctrl = 58, kw_alt = 50, kw_v = 40;
    int gap = 22;
    int key_h = 40;
    int key_y = 116;
    int total = kw_ctrl + gap + kw_alt + gap + kw_v;
    int kx = (rc.right - total) / 2;

    draw_key_90s(hdc, kx, key_y, kw_ctrl, key_h, L"Ctrl", key_font);
    SelectObject(hdc, plus_font);
    RECT plus1 = { kx + kw_ctrl, key_y, kx + kw_ctrl + gap, key_y + key_h };
    DrawTextW(hdc, L"+", -1, &plus1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    draw_key_90s(hdc, kx + kw_ctrl + gap, key_y, kw_alt, key_h, L"Alt", key_font);
    SelectObject(hdc, plus_font);
    RECT plus2 = { kx + kw_ctrl + gap + kw_alt, key_y, kx + kw_ctrl + gap + kw_alt + gap, key_y + key_h };
    DrawTextW(hdc, L"+", -1, &plus2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    draw_key_90s(hdc, kx + kw_ctrl + gap + kw_alt + gap, key_y, kw_v, key_h, L"V", key_font);

    SelectObject(hdc, body_font);
    RECT hint_rc = rc;
    hint_rc.top = 174;
    hint_rc.bottom = hint_rc.top + 22;
    DrawTextW(hdc, L"Minimize to send to tray.", -1, &hint_rc, DT_CENTER | DT_SINGLELINE);

    HFONT ver_font = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    SelectObject(hdc, ver_font);
    SetTextColor(hdc, RGB(112, 112, 112));
    RECT ver_rc = rc;
    ver_rc.top    = rc.bottom - 18;
    ver_rc.bottom = rc.bottom - 4;
    ver_rc.right -= 8;
    DrawTextW(hdc, L"v" APP_VERSION, -1, &ver_rc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, RGB(0, 0, 0));

    SelectObject(hdc, old);
    DeleteObject(title_font);
    DeleteObject(body_font);
    DeleteObject(plus_font);
    DeleteObject(key_font);
    DeleteObject(ver_font);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            draw_window(hwnd);
            return 0;
        case WM_HOTKEY:
            if (wp == HOTKEY_ID) type_clipboard();
            return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hwnd, SW_HIDE);
                add_tray_icon(hwnd);
                return 0;
            }
            break;
        case WM_TRAYICON:
            if (LOWORD(lp) == WM_LBUTTONUP)      restore_window(hwnd);
            else if (LOWORD(lp) == WM_RBUTTONUP) show_tray_menu(hwnd);
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDM_SHOW: restore_window(hwnd); break;
                case IDM_EXIT: DestroyWindow(hwnd);  break;
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            UnregisterHotKey(hwnd, HOTKEY_ID);
            remove_tray_icon(hwnd);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    (void)hPrev; (void)cmd; (void)show;

    HANDLE mtx = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    DWORD mtx_err = GetLastError();
    if (mtx && mtx_err == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(CLASS_NAME, NULL);
        if (existing) {
            ShowWindow(existing, SW_SHOW);
            SetForegroundWindow(existing);
        }
        CloseHandle(mtx);
        return 0;
    }

    g_icon_large = create_key_icon(32);
    g_icon_small = create_key_icon(16);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon         = g_icon_large ? g_icon_large : LoadIconW(NULL, IDI_APPLICATION);
    if (!RegisterClassW(&wc)) {
        if (mtx) CloseHandle(mtx);
        return 1;
    }

    int w = 380, h = 260;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h, NULL, NULL, hInst, NULL);
    if (!hwnd) {
        if (mtx) CloseHandle(mtx);
        return 1;
    }

    if (!RegisterHotKey(hwnd, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'V')) {
        MessageBoxW(hwnd,
            L"Could not register Ctrl+Alt+V hotkey.\nAnother app may be using it.",
            L"PasteAsKeystrokes", MB_ICONWARNING);
    }

    if (g_icon_small) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_icon_small);
    if (g_icon_large) SendMessageW(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)g_icon_large);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (mtx) CloseHandle(mtx);
    if (g_icon_large) DestroyIcon(g_icon_large);
    if (g_icon_small) DestroyIcon(g_icon_small);
    return (int)msg.wParam;
}
