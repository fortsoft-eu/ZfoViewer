/**
 * This is open-source software licensed under the terms of the MIT License.
 *
 * Copyright (c) 2026 Petr Červinka - FortSoft <cervinka@fortsoft.eu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **
 * Last modified for version 1.0.0.0
 */

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include "ZfoViewer.h"
#include "MainForm.h"
#include "resource.h"
#include "ImageExport.h"
#include "SystemDialogTemplate.h"
#include "PrintSettings.h"
#include <commctrl.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <dwrite.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <usp10.h>
#include <uxtheme.h>
#include <cstdlib>
#include <algorithm>
#include <cwchar>
#include <cstring>
#include <stdexcept>
#include <new>

#undef MessageBoxW

/// Records measured ascent, descent and external leading for wrapped interface text.
struct UiTextLine {
    LONG ascent;
    LONG descent;
    LONG gap;
};

/// Retains Uniscribe word-break analysis and the exact UTF-16 text analyzed.
struct InformationWordBreakState {
    SCRIPT_STRING_ANALYSIS analysis = nullptr;
    std::wstring text;
};

/// Collects an immutable RTF snapshot without changing the RichEdit's selection or viewport.
struct PrintRtfCapture {
    std::vector<BYTE> bytes;
};

/// Native ideal-button-size message and cached analysis used by information edits.
static const UINT buttonGetIdealSize = BCM_GETIDEALSIZE;
constexpr int COMBO_BOX_MAX_VISIBLE_ITEMS = 13;
constexpr UINT_PTR COMBO_BOX_DROPDOWN_SUBCLASS_ID = 0xF505;

static constexpr const wchar_t* WindowIconProperties[] = { L"ZfoViewer.SmallIcon", L"ZfoViewer.LargeIcon" };
static constexpr int WindowIconTypes[] = { ICON_SMALL, ICON_BIG };

/// Option-bit positions matching the established viewer settings, followed by the smooth-scrolling bit.
static constexpr DWORD MultipleInstancesOption = 0x04;
static constexpr DWORD CloseTabsOnDoubleClickOption = 0x40;
static constexpr DWORD SmoothScrollingOption = 0x80;

/// Disabled-color bits keep highlighting enabled when older preferences contain neither new flag.
static constexpr DWORD DisableMessageColorsOption = 0x100;
static constexpr DWORD DisableSourceColorsOption = 0x200;

/// Registry namespace for application preferences, recent files and Help placement.
static constexpr wchar_t SettingsKey[] = L"Software\\FortSoft\\ZfoViewer";

static InformationWordBreakState informationWordBreakState;

/// Image-export diagnostics indexed in the same order as the six image formats.
static const TextId imageErrors[] = {
    TXT_BMP_EXPORT_FAILED,
    TXT_GIF_EXPORT_FAILED,
    TXT_JPEG_EXPORT_FAILED,
    TXT_PNG_EXPORT_FAILED,
    TXT_TIFF_EXPORT_FAILED,
    TXT_WEBP_EXPORT_FAILED
};

/// Export filters in menu order: five text encodings, ODT, DOCX, RTF, HTML, XHTML, XML and six image formats.
const ExportChoice exportChoices[] = {
    { 1, TXT_TEXT_FORMAT, L"*.txt", L"txt" },
    { 2, TXT_TEXT_UTF8_FORMAT, L"*.txt", L"txt" },
    { 3, TXT_TEXT_UTF8_BOM_FORMAT, L"*.txt", L"txt" },
    { 4, TXT_TEXT_UTF16_BE_FORMAT, L"*.txt", L"txt" },
    { 5, TXT_TEXT_UTF16_LE_FORMAT, L"*.txt", L"txt" },
    { 6, TXT_ODT_FORMAT, L"*.odt", L"odt" },
    { 7, TXT_DOCX_FORMAT, L"*.docx", L"docx" },
    { 8, TXT_RTF_FORMAT, L"*.rtf", L"rtf" },
    { 9, TXT_HTML_FORMAT, L"*.html", L"html" },
    { 10, TXT_XHTML_FORMAT, L"*.xhtml", L"xhtml" },
    { 11, TXT_XML_FORMAT, L"*.xml", L"xml" },
    { 12, TXT_BMP_FORMAT, L"*.bmp", L"bmp" },
    { 13, TXT_GIF_FORMAT, L"*.gif", L"gif" },
    { 14, TXT_JPEG_FORMAT, L"*.jpg", L"jpg" },
    { 15, TXT_PNG_FORMAT, L"*.png", L"png" },
    { 16, TXT_TIFF_FORMAT, L"*.tif", L"tif" },
    { 17, TXT_WEBP_FORMAT, L"*.webp", L"webp" }
};

/// Uses the application's product caption for diagnostics raised by shared components.
int WINAPI ComponentMessageBoxW(HWND owner, LPCWSTR text, LPCWSTR title, UINT type) {
    return MessageBoxW(owner, text, title && !lstrcmpW(title, L"MediaInfo") ? L"ZFO Viewer" : title, type);
}

/// Reports whether visual styles are active for Windows and the current application.
bool AreVisualStylesActive() {
    return IsThemeActive() && IsAppThemed();
}

/// Returns the effective DPI through the shared system-dialog implementation.
UINT WindowDpi(HWND window) {
    return GetSystemDialogDpi(window);
}

/// Creates an owned system dialog font for the target window's DPI.
HFONT CreateUiFont(HWND window) {
    return CreateSystemDialogFont(window);
}

/// Removes and destroys the small and large icons owned through window properties.
void ClearWindowIcons(HWND window) {
    for (const wchar_t* property : WindowIconProperties) {
        HICON icon = static_cast<HICON>(RemovePropW(window, property));
        if (icon) {
            DestroyIcon(icon);
        }
    }
}

/// Reloads small and large caption icons from the requested resource at the window's current DPI.
void UpdateWindowIcons(HWND window, UINT resource) {
    ClearWindowIcons(window);
    UINT dpi = WindowDpi(window);
    UINT systemDpi = WindowDpi(nullptr);
    for (int type : WindowIconTypes) {
        int width = GetSystemMetrics(type == ICON_SMALL ? SM_CXSMICON : SM_CXICON);
        int height = GetSystemMetrics(type == ICON_SMALL ? SM_CYSMICON : SM_CYICON);
        HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(resource),
            IMAGE_ICON, MulDiv(width, dpi, systemDpi), MulDiv(height, dpi, systemDpi), 0));
        if (icon) {
            SetPropW(window, type == ICON_SMALL ? L"ZfoViewer.SmallIcon" : L"ZfoViewer.LargeIcon", icon);
            SendMessageW(window, WM_SETICON, type, reinterpret_cast<LPARAM>(icon));
        }
    }
}

/// Returns the nearest monitor's work area, using the system work area if monitor information is unavailable.
RECT WindowWorkArea(const RECT& rectangle) {
    MONITORINFO monitor{ sizeof(monitor) };
    RECT bounds = rectangle;
    if (GetMonitorInfoW(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor)) {
        return monitor.rcWork;
    }
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &bounds, 0);
    return bounds;
}

/// Copies edit text into a caller-owned process-heap buffer, preserving RichEdit UTF-16 selection offsets.
wchar_t* CopyControlText(HWND control) {
    wchar_t className[32]{};
    GetClassNameW(control, className, ARRAYSIZE(className));
    bool rich = _wcsnicmp(className, L"RichEdit", 8) == 0;
    GETTEXTLENGTHEX lengthRequest{ GTL_PRECISE | GTL_NUMCHARS, 1200 };
    LRESULT length = rich
        ? SendMessageW(control, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&lengthRequest), 0)
        : GetWindowTextLengthW(control);
    if (length < 0 || length >= MAXDWORD / sizeof(wchar_t) - 1) {
        return nullptr;
    }
    DWORD bytes = static_cast<DWORD>((length + 1) * sizeof(wchar_t));
    wchar_t* text = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes));
    if (text) {
        if (rich) {
            GETTEXTEX request{ bytes, GT_DEFAULT, 1200, nullptr, nullptr };
            SendMessageW(control, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&request),
                reinterpret_cast<LPARAM>(text));
        } else {
            GetWindowTextW(control, text, static_cast<int>(length + 1));
        }
    }
    return text;
}

/// Copies Unicode text to the clipboard, transferring buffer ownership only after a successful copy.
bool CopyTextToClipboard(HWND owner, const wchar_t* text) {
    SIZE_T bytes = (static_cast<SIZE_T>(lstrlenW(text)) + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) {
        return false;
    }
    void* destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        return false;
    }
    CopyMemory(destination, text, bytes);
    GlobalUnlock(memory);
    bool success = false;
    if (OpenClipboard(owner)) {
        success = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory);
        CloseClipboard();
    }
    if (!success) {
        GlobalFree(memory);
    }
    return success;
}

/// Converts dialog units to pixels using the supplied font's native base-unit measurements.
SIZE DialogUnits(HFONT font, int x, int y) {
    HDC dc = GetDC(nullptr);
    SIZE extent = SIZE{};
    TEXTMETRICW metrics = TEXTMETRICW{};
    if (!dc) {
        return SIZE{ x, y };
    }
    HGDIOBJ old = SelectObject(dc, font);
    GetTextExtentPoint32W(dc, L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &extent);
    GetTextMetricsW(dc, &metrics);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    int baseX = (extent.cx / 26 + 1) / 2;
    return SIZE{ MulDiv(x, baseX, 4), MulDiv(y, metrics.tmHeight, 8) };
}

/// Measures the button's ideal content size and retains the supplied native minimum dimensions.
SIZE PreferredButtonSize(HWND button, const SIZE& minimum) {
    SIZE size = SIZE{};
    if (!SendMessageW(button, buttonGetIdealSize, 0, reinterpret_cast<LPARAM>(&size))) {
        wchar_t* text = CopyControlText(button);
        HDC dc = GetDC(button);
        if (text && dc) {
            HFONT font = reinterpret_cast<HFONT>(SendMessageW(button, WM_GETFONT, 0, 0));
            HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;
            RECT bounds = RECT{};
            DrawTextW(dc, text, -1, &bounds, DT_CALCRECT | DT_SINGLELINE);
            SIZE padding = DialogUnits(font, 12, 0);
            size.cx = bounds.right + padding.cx + 2 * GetSystemMetrics(SM_CXEDGE);
            size.cy = bounds.bottom + 2 * GetSystemMetrics(SM_CYEDGE);
            if (previous) {
                SelectObject(dc, previous);
            }
        }
        if (dc) {
            ReleaseDC(button, dc);
        }
        if (text) {
            HeapFree(GetProcessHeap(), 0, text);
        }
    }
    if (size.cx < minimum.cx) {
        size.cx = minimum.cx;
    }
    if (size.cy < minimum.cy) {
        size.cy = minimum.cy;
    }
    return size;
}

/// Returns a control's window bounds mapped into its parent's client coordinates.
RECT ControlRect(HWND window, HWND parent) {
    RECT rectangle = RECT{};
    GetWindowRect(window, &rectangle);
    MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<POINT*>(&rectangle), 2);
    return rectangle;
}

/// Positions and sizes a control without activation, Z-order changes or copied stale pixels.
void PlaceControl(HWND window, int x, int y, int width, int height) {
    SetWindowPos(window, nullptr, x, y, width > 0 ? width : 1, height > 0 ? height : 1,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

/// Reads text metrics from the control's current font and reports whether measurement succeeded.
bool ReadControlMetrics(HWND control, TEXTMETRICW* metrics) {
    HDC dc = GetDC(control);
    if (!dc) {
        return false;
    }
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
    HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
    bool success = GetTextMetricsW(dc, metrics) != FALSE;
    if (old) {
        SelectObject(dc, old);
    }
    ReleaseDC(control, dc);
    return success;
}

/// Returns the native selected-text rectangle used for combo-box baseline alignment.
bool ComboBoxTextRect(HWND combo, RECT* rectangle) {
    COMBOBOXINFO information = {};
    information.cbSize = sizeof(information);
    if (!GetComboBoxInfo(combo, &information)) {
        return false;
    }
    *rectangle = information.rcItem;
    return true;
}

/// Fits native combo selection and list-item heights to its current font and system borders.
void UpdateComboBoxMetrics(HWND combo) {
    LRESULT selectionHeight = SendMessageW(combo, CB_GETITEMHEIGHT, static_cast<WPARAM>(-1), 0);
    TEXTMETRICW metrics = TEXTMETRICW{};
    if (selectionHeight == CB_ERR || !ReadControlMetrics(combo, &metrics)) {
        return;
    }
    LONG height = metrics.tmHeight + 2 * GetSystemMetrics(SM_CYBORDER);
    if (selectionHeight != height) {
        SendMessageW(combo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), height);
    }
    if (SendMessageW(combo, CB_GETITEMHEIGHT, 0, 0) != metrics.tmHeight) {
        SendMessageW(combo, CB_SETITEMHEIGHT, 0, metrics.tmHeight);
    }
}

/// Adjusts a dropdown's pending position and size to the monitor work area before it becomes visible.
/// Removes its subclass when the native list window is destroyed.
static LRESULT CALLBACK ComboBoxDropDownSubclassProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR subclassId, DWORD_PTR referenceData) {
    if (msg == WM_WINDOWPOSCHANGING) {
        WINDOWPOS* position = reinterpret_cast<WINDOWPOS*>(lParam);
        HWND combo = reinterpret_cast<HWND>(referenceData);
        HMONITOR monitor = MonitorFromWindow(combo, MONITOR_DEFAULTTONEAREST);
        MONITORINFO information = {};
        information.cbSize = sizeof(information);
        RECT current = {};
        RECT comboRect = {};
        if (position != nullptr
                && GetMonitorInfoW(monitor, &information)
                && GetWindowRect(window, &current)
                && GetWindowRect(combo, &comboRect)) {
            LONG workWidth = information.rcWork.right - information.rcWork.left;
            if (workWidth > 0) {
                LONG currentWidth = position->flags & SWP_NOSIZE
                    ? current.right - current.left
                    : position->cx;
                LONG minimumWidth = std::max(1L, comboRect.right - comboRect.left);
                LONG width = std::max(minimumWidth, std::min(currentWidth, workWidth));
                LONG left = position->flags & SWP_NOMOVE ? current.left : position->x;
                LONG maximumLeft = information.rcWork.right - width;
                LONG minimumLeft = std::min(information.rcWork.left, maximumLeft);
                LONG fittedLeft = std::clamp(left, minimumLeft, maximumLeft);
                if (width != currentWidth) {
                    position->cx = width;
                    if (position->flags & SWP_NOSIZE) {
                        position->cy = current.bottom - current.top;
                    }
                    position->flags &= ~SWP_NOSIZE;
                }
                if (fittedLeft != left) {
                    position->x = fittedLeft;
                    if (position->flags & SWP_NOMOVE) {
                        position->y = current.top;
                    }
                    position->flags &= ~SWP_NOMOVE;
                }
            }
        }
    } else if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(window, ComboBoxDropDownSubclassProc, subclassId);
    }
    return DefSubclassProc(window, msg, wParam, lParam);
}

/// Measures current items in the dropdown font immediately before opening, adding borders, padding, and
/// scrollbar space. Limits the list to 13 visible items and keeps it at least as wide as its combo within
/// the monitor work area.
void UpdateComboBoxDropDownSize(HWND combo) {
    if (combo == nullptr) {
        return;
    }
    SendMessageW(combo, CB_SETMINVISIBLE, COMBO_BOX_MAX_VISIBLE_ITEMS, 0);
    COMBOBOXINFO information = {};
    information.cbSize = sizeof(information);
    HWND list = combo;
    if (GetComboBoxInfo(combo, &information) && information.hwndList != nullptr) {
        list = information.hwndList;
        SetWindowSubclass(information.hwndList, ComboBoxDropDownSubclassProc, COMBO_BOX_DROPDOWN_SUBCLASS_ID,
            reinterpret_cast<DWORD_PTR>(combo));
    }
    HDC dc = GetDC(list);
    if (dc == nullptr) {
        return;
    }
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(list, WM_GETFONT, 0, 0));
    if (font == nullptr) {
        font = reinterpret_cast<HFONT>(SendMessageW(combo, WM_GETFONT, 0, 0));
    }
    if (font == nullptr) {
        ReleaseDC(list, dc);
        return;
    }
    HGDIOBJ previousFont = SelectObject(dc, font);
    LONG maximumWidth = 0;
    int count = static_cast<int>(SendMessageW(combo, CB_GETCOUNT, 0, 0));
    for (int index = 0; index < count; index++) {
        int length = static_cast<int>(SendMessageW(combo, CB_GETLBTEXTLEN, index, 0));
        if (length <= 0) {
            continue;
        }
        std::wstring text(length + 1, L'\0');
        if (SendMessageW(combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(text.data())) == CB_ERR) {
            continue;
        }
        SIZE size = {};
        if (GetTextExtentPoint32W(dc, text.c_str(), length, &size)) {
            ABC spacing = {};
            wchar_t lastCharacter = text[length - 1];
            if (GetCharABCWidthsW(dc, lastCharacter, lastCharacter, &spacing)) {
                size.cx += std::max(0, -spacing.abcC);
            }
            maximumWidth = std::max(maximumWidth, size.cx);
        }
    }
    SelectObject(dc, previousFont);
    ReleaseDC(list, dc);
    LONG width = maximumWidth + 2 * GetSystemMetrics(SM_CXEDGE);
    DWORD listStyle = WS_BORDER;
    DWORD listExtendedStyle = 0;
    if (information.hwndList != nullptr) {
        listStyle = static_cast<DWORD>(GetWindowLongPtrW(information.hwndList, GWL_STYLE));
        listExtendedStyle = static_cast<DWORD>(GetWindowLongPtrW(information.hwndList, GWL_EXSTYLE));
    }
    RECT listRect = { 0, 0, width, 1 };
    if (AdjustWindowRectEx(&listRect, listStyle, FALSE, listExtendedStyle)) {
        width = listRect.right - listRect.left;
    } else {
        width += 2 * GetSystemMetrics(SM_CXBORDER);
    }
    width += GetSystemMetrics(SM_CXVSCROLL);
    RECT rect = {};
    if (GetWindowRect(combo, &rect)) {
        width = std::max(width, rect.right - rect.left);
    }
    SendMessageW(combo, CB_SETDROPPEDWIDTH, width, 0);
}

/// Calculates the rendered text baseline in screen coordinates from native control content and font metrics.
LONG ControlTextBaseline(HWND control) {
    RECT rectangle = RECT{};
    GetClientRect(control, &rectangle);
    POINT origin = POINT{};
    ClientToScreen(control, &origin);
    TEXTMETRICW metrics = TEXTMETRICW{};
    ReadControlMetrics(control, &metrics);
    wchar_t name[32];
    GetClassNameW(control, name, 32);
    LONG textTop = 0;
    if (CompareStringOrdinal(name, -1, L"EDIT", -1, TRUE) == CSTR_EQUAL) {
        SendMessageW(control, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&rectangle));
        textTop = rectangle.top;
    } else if (CompareStringOrdinal(name, -1, L"COMBOBOX", -1, TRUE) == CSTR_EQUAL) {
        if (ComboBoxTextRect(control, &rectangle)) {
            textTop = rectangle.top + (rectangle.bottom - rectangle.top - metrics.tmHeight) / 2;
        }
    } else if (CompareStringOrdinal(name, -1, L"BUTTON", -1, TRUE) == CSTR_EQUAL) {
        HTHEME theme = GetWindowTheme(control);
        LONG textHeight = metrics.tmHeight;
        LONG type = GetWindowLongW(control, GWL_STYLE) & BS_TYPEMASK;
        if (!theme) {
            rectangle.bottom -= GetSystemMetrics(SM_CYBORDER);
        } else if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON) {
            RECT content = RECT{};
            if (SUCCEEDED(GetThemeBackgroundContentRect(theme, nullptr, 1, 1, &rectangle, &content))) {
                rectangle = content;
            }
        } else if (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX) {
            SIZE glyph = SIZE{};
            if (SUCCEEDED(GetThemePartSize(theme, nullptr, 3, 1, nullptr, TS_TRUE, &glyph))
                    && glyph.cy > textHeight) {
                textHeight = glyph.cy;
            }
        }
        textTop = rectangle.top + (rectangle.bottom - rectangle.top - textHeight) / 2;
    } else if (CompareStringOrdinal(name, -1, L"STATIC", -1, TRUE) == CSTR_EQUAL
            && GetWindowLongW(control, GWL_STYLE) & SS_CENTERIMAGE) {
        textTop = (rectangle.bottom - rectangle.top - metrics.tmHeight) / 2;
    }
    return origin.y + textTop + metrics.tmAscent;
}

/// Moves a control vertically to match the reference control's rendered text baseline.
void AlignControlBaseline(HWND control, HWND reference) {
    LONG distance = ControlTextBaseline(reference) - ControlTextBaseline(control);
    if (distance) {
        RECT bounds = ControlRect(control, GetParent(control));
        SetWindowPos(control, nullptr, bounds.left, bounds.top + distance, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
    }
}

/// Fits an edit's text inset and height to current font and theme metrics, including edits inside combo
/// boxes.
void UpdateEditMetrics(HWND edit, HWND enclosingCombo) {
    HWND frameWindow = enclosingCombo ? enclosingCombo : edit;
    RECT bounds = RECT{};
    POINT client = POINT{};
    if (!GetWindowRect(frameWindow, &bounds) || !ClientToScreen(edit, &client)) {
        return;
    }
    TEXTMETRICW metrics = TEXTMETRICW{};
    if (!ReadControlMetrics(edit, &metrics)) {
        return;
    }
    LONG contentLeft = GetSystemMetrics(SM_CXEDGE) + GetSystemMetrics(SM_CXBORDER);
    LONG contentTop = GetSystemMetrics(SM_CYEDGE) + GetSystemMetrics(SM_CYBORDER);
    LONG contentBottom = contentTop;
    if (GetWindowTheme(frameWindow)) {
        HTHEME theme = OpenThemeData(nullptr, L"Combobox");
        if (theme) {
            RECT frame = RECT{ 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top };
            RECT content = RECT{};
            if (SUCCEEDED(GetThemeBackgroundContentRect(theme, nullptr, 5, 1, &frame, &content))) {
                contentLeft = content.left;
                contentTop = content.top;
                contentBottom = frame.bottom - content.bottom;
            }
            CloseThemeData(theme);
        }
    }
    LONG borderWidth = client.x - bounds.left - (enclosingCombo ? GetSystemMetrics(SM_CXBORDER) : 0);
    LONG leftMargin = contentLeft > borderWidth ? contentLeft - borderWidth : 0;
    DWORD margins = static_cast<DWORD>(SendMessageW(edit, EM_GETMARGINS, 0, 0));
    if (LOWORD(margins) != leftMargin) {
        SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(leftMargin, 0));
    }
    LONG height = metrics.tmHeight + contentTop + contentBottom + 2 * GetSystemMetrics(SM_CYBORDER);
    if (!enclosingCombo && height != bounds.bottom - bounds.top) {
        SetWindowPos(edit, nullptr, 0, 0, bounds.right - bounds.left, height,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

/// Uses Windows Unicode character classification to identify whitespace.
bool IsTextWhitespace(wchar_t character) {
    WORD type = 0;
    return GetStringTypeW(CT_CTYPE1, &character, 1, &type) && type & C1_SPACE;
}

/// Releases cached Uniscribe analysis and its associated UTF-16 text.
void ResetInformationWordBreak() {
    if (informationWordBreakState.analysis != nullptr) {
        if (FAILED(ScriptStringFree(&informationWordBreakState.analysis))) {
            OutputDebugStringW(L"ScriptStringFree failed while clearing Unicode word-break analysis.\r\n");
        }
        informationWordBreakState.analysis = nullptr;
    }
    informationWordBreakState.text.clear();
}

/// Provides native edit word movement from cached Unicode analysis, with a character-class fallback on
/// failure.
int CALLBACK InformationWordBreakProc(wchar_t* text, int position, int length, int code) {
    if (length <= 0) {
        return 0;
    }
    position = std::clamp(position, 0, length);
    if (informationWordBreakState.text.size() != static_cast<size_t>(length)
            || wmemcmp(informationWordBreakState.text.c_str(), text, length) != 0) {
        ResetInformationWordBreak();
        HRESULT status = S_OK;
        try {
            informationWordBreakState.text.assign(text, length);
            status = ScriptStringAnalyse(nullptr, informationWordBreakState.text.c_str(),
                length, 0, -1, SSA_BREAK, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
                &informationWordBreakState.analysis);
        } catch (const std::bad_alloc& error) {
            status = E_OUTOFMEMORY;
            OutputDebugStringA("Unicode word-break allocation failed; using character classification: ");
            OutputDebugStringA(error.what());
            OutputDebugStringA("\r\n");
        }
        if (FAILED(status)) {
            ResetInformationWordBreak();
            if (code == WB_ISDELIMITER) {
                return position < length && IsUnicodeWhitespace(text[position]);
            }
            return code == WB_LEFT ? 0 : length;
        }
    }
    const SCRIPT_LOGATTR* attributes = ScriptString_pLogAttr(informationWordBreakState.analysis);
    if (code == WB_ISDELIMITER) {
        return position < length && attributes[position].fWhiteSpace;
    }
    if (code == WB_LEFT) {
        if (position == length) {
            position--;
        }
        while (position > 0
            && (!attributes[position].fCharStop
                || !attributes[position].fSoftBreak && !attributes[position - 1].fWhiteSpace)) {
            position--;
        }
    } else if (code == WB_RIGHT) {
        if (position < length) {
            position++;
        }
        while (position < length
            && (!attributes[position].fCharStop
                || !attributes[position].fSoftBreak && !attributes[position - 1].fWhiteSpace)) {
            position++;
        }
    }
    return position;
}



/// Derives text-line metrics from the selected font and glyph extents without clipping the supplied text.
static UiTextLine GetUiTextLine(HDC dc, const std::wstring& text) {
    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    UiTextLine line = { metrics.tmAscent, metrics.tmDescent, metrics.tmExternalLeading };
    UINT size = GetOutlineTextMetricsW(dc, 0, nullptr);
    if (size == 0) {
        return line;
    }
    std::vector<BYTE> data(size);
    OUTLINETEXTMETRICW* outline = reinterpret_cast<OUTLINETEXTMETRICW*>(data.data());
    if (GetOutlineTextMetricsW(dc, size, outline) == 0) {
        return line;
    }
    UiTextLine typographic = {
        outline->otmAscent,
        std::abs(outline->otmDescent),
        static_cast<LONG>(outline->otmLineGap)
    };
    MAT2 transform = {
        FIXED{ 0, 1 },
        FIXED{ 0, 0 },
        FIXED{ 0, 0 },
        FIXED{ 0, 1 }
    };
    for (wchar_t character : text) {
        if (IsUnicodeWhitespace(character)) {
            continue;
        }
        GLYPHMETRICS glyph = {};
        if (GetGlyphOutlineW(dc, character, GGO_METRICS, &glyph, 0, nullptr, &transform) != GDI_ERROR) {
            typographic.ascent = std::max(typographic.ascent, glyph.gmptGlyphOrigin.y);
            typographic.descent = std::max(typographic.descent,
                static_cast<LONG>(glyph.gmBlackBoxY) - glyph.gmptGlyphOrigin.y);
        }
    }
    if (typographic.ascent + typographic.descent + typographic.gap
            < metrics.tmHeight + metrics.tmExternalLeading) {
        line = typographic;
    }
    return line;
}

/// Removes mnemonic markers from a caption while preserving escaped ampersands.
static std::wstring RemoveCaptionMnemonic(const std::wstring& caption) {
    std::wstring text;
    for (size_t index = 0; index < caption.size(); index++) {
        if (caption[index] != L'&') {
            text += caption[index];
        } else if (index + 1 < caption.size() && caption[index + 1] == L'&') {
            text += L'&';
            index++;
        }
    }
    return text;
}

/// Draws or measures descriptive text using the same word wrapping and current font's line height.
static int DrawWordWrappedText(HDC dc, const std::wstring& text, const RECT& bounds,
        UINT prefix = DT_NOPREFIX) {
    TEXTMETRICW metrics = {};
    if (!GetTextMetricsW(dc, &metrics)) {
        return 0;
    }
    UiTextLine layout = GetUiTextLine(dc, text);
    int lineHeight = std::max(1L, layout.ascent + layout.descent + layout.gap);
    bool measure = (prefix & DT_CALCRECT) != 0;
    int saved = SaveDC(dc);
    if (!measure) {
        IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    }
    int maximumWidth = bounds.right - bounds.left;
    int y = bounds.top;
    size_t position = 0;
    std::wstring line;
    while (position < text.size() && (measure || y < bounds.bottom)) {
        if (text[position] == L'\r' || text[position] == L'\n') {
            if (!measure && !line.empty()) {
                RECT lineRect = {
                    bounds.left,
                    y + layout.ascent - metrics.tmAscent,
                    bounds.right,
                    y + layout.ascent + metrics.tmDescent
                };
                DrawTextW(dc, line.c_str(), -1, &lineRect, DT_LEFT | DT_TOP | DT_SINGLELINE | prefix);
            }
            line.clear();
            wchar_t newline = text[position++];
            if (position < text.size() && newline == L'\r' && text[position] == L'\n') {
                position++;
            }
            y += lineHeight;
            continue;
        }
        while (position < text.size()
                && IsUnicodeWhitespace(text[position])
                && text[position] != L'\r'
                && text[position] != L'\n') {
            position++;
        }
        size_t wordStart = position;
        while (position < text.size() && !IsUnicodeWhitespace(text[position])) {
            position++;
        }
        if (wordStart == position) {
            continue;
        }
        std::wstring word = text.substr(wordStart, position - wordStart);
        std::wstring candidate = line.empty() ? word : line + L" " + word;
        SIZE extent = {};
        std::wstring measured = prefix & DT_NOPREFIX ? candidate : RemoveCaptionMnemonic(candidate);
        GetTextExtentPoint32W(dc, measured.c_str(), static_cast<int>(measured.size()), &extent);
        if (!line.empty() && extent.cx > maximumWidth) {
            if (!measure) {
                RECT lineRect = {
                    bounds.left,
                    y + layout.ascent - metrics.tmAscent,
                    bounds.right,
                    y + layout.ascent + metrics.tmDescent
                };
                DrawTextW(dc, line.c_str(), -1, &lineRect, DT_LEFT | DT_TOP | DT_SINGLELINE | prefix);
            }
            y += lineHeight;
            line = word;
        } else {
            line = candidate;
        }
    }
    if (!line.empty() && (measure || y < bounds.bottom)) {
        if (!measure) {
            RECT lineRect = {
                bounds.left,
                y + layout.ascent - metrics.tmAscent,
                bounds.right,
                y + layout.ascent + metrics.tmDescent
            };
            DrawTextW(dc, line.c_str(), -1, &lineRect, DT_LEFT | DT_TOP | DT_SINGLELINE | prefix);
        }
        y += lineHeight;
    }
    RestoreDC(dc, saved);
    return y - bounds.top;
}

/// Paints owner-drawn product text using the control font, enabled-state color and parent background.
INT_PTR DrawWrappedTextItem(const DRAWITEMSTRUCT* item) {
    int savedState = SaveDC(item->hDC);
    DrawThemeParentBackground(item->hwndItem, item->hDC, nullptr);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, GetSysColor(IsWindowEnabled(item->hwndItem) ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(item->hwndItem, WM_GETFONT, 0, 0));
    if (font != nullptr) {
        SelectObject(item->hDC, font);
    }
    std::wstring text = GetControlText(item->hwndItem);
    RECT textRect = item->rcItem;
    UINT prefix = DT_NOPREFIX;
    DrawWordWrappedText(item->hDC, text, textRect, prefix);
    RestoreDC(item->hDC, savedState);
    return TRUE;
}

/// Copies a rich-text stream into memory and reports allocation failures to the native streaming API.
static DWORD CALLBACK CapturePrintRtf(DWORD_PTR cookie, LPBYTE bytes, LONG count, LONG* copied) {
    PrintRtfCapture* captured = reinterpret_cast<PrintRtfCapture*>(cookie);
    try {
        captured->bytes.insert(captured->bytes.end(), bytes, bytes + count);
        *copied = count;
        return ERROR_SUCCESS;
    } catch (const std::exception& error) {
        OutputDebugStringA("Capturing RTF for printing failed: ");
        OutputDebugStringA(error.what());
        OutputDebugStringA("\n");
        *copied = 0;
        return ERROR_NOT_ENOUGH_MEMORY;
    }
}

/// Reads one native character-format run through TOM without moving the visible selection.
static bool CapturePrintRun(ITextRange* range, UINT start, UINT end, PrintTextRun& run) {
    Microsoft::WRL::ComPtr<ITextFont> font;
    if (FAILED(range->GetFont(&font))) {
        return false;
    }
    BSTR rawName = nullptr;
    if (FAILED(font->GetName(&rawName))) {
        return false;
    }
    std::unique_ptr<wchar_t, decltype(&SysFreeString)> name(rawName, SysFreeString);
    float size = 0;
    LONG color = 0;
    LONG bold = tomFalse;
    LONG italic = tomFalse;
    LONG underline = tomFalse;
    LONG strike = tomFalse;
    bool captured = SUCCEEDED(font->GetSize(&size))
        && SUCCEEDED(font->GetForeColor(&color))
        && SUCCEEDED(font->GetBold(&bold))
        && SUCCEEDED(font->GetItalic(&italic))
        && SUCCEEDED(font->GetUnderline(&underline))
        && SUCCEEDED(font->GetStrikeThrough(&strike));
    if (!captured || !name || !*name || size <= 0) {
        return false;
    }
    run.start = start;
    run.length = end - start;
    run.color = color == tomAutoColor ? RGB(0, 0, 0) : static_cast<COLORREF>(color);
    run.heightTwips = static_cast<LONG>(size * 20.0f + 0.5f);
    run.weight = bold == tomTrue ? FW_BOLD : FW_NORMAL;
    run.italic = italic == tomTrue;
    run.underline = underline != tomFalse;
    run.strikeOut = strike == tomTrue;
    wcsncpy_s(run.family, name.get(), _TRUNCATE);
    return true;
}

/// Applies captured character formats to a complete modern layout or a page-relative text range.
static HRESULT FormatPrintLayout(const PrintJob* job, IUnknown* text, IUnknown* drawing, UINT start, UINT length) {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target;
    HRESULT result = text->QueryInterface(IID_PPV_ARGS(&layout));
    if (SUCCEEDED(result)) {
        result = drawing->QueryInterface(IID_PPV_ARGS(&target));
    }
    for (UINT index = 0; SUCCEEDED(result) && index < job->formatCount; ++index) {
        const PrintTextRun& run = job->formats[index];
        UINT first = std::max(start, run.start);
        UINT end = std::min(start + length, run.start + run.length);
        if (first >= end) {
            continue;
        }
        DWRITE_TEXT_RANGE range{ first - start, end - first };
        result = layout->SetFontFamilyName(run.family, range);
        if (SUCCEEDED(result)) {
            result = layout->SetFontSize(run.heightTwips / 15.0f, range);
        }
        if (SUCCEEDED(result)) {
            result = layout->SetFontWeight(static_cast<DWRITE_FONT_WEIGHT>(run.weight), range);
        }
        if (SUCCEEDED(result)) {
            DWRITE_FONT_STYLE style = run.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
            result = layout->SetFontStyle(style, range);
        }
        if (SUCCEEDED(result)) {
            result = layout->SetUnderline(run.underline, range);
        }
        if (SUCCEEDED(result)) {
            result = layout->SetStrikethrough(run.strikeOut, range);
        }
        D2D1_COLOR_F color{ GetRValue(run.color) / 255.0f, GetGValue(run.color) / 255.0f,
            GetBValue(run.color) / 255.0f, 1.0f };
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
        if (SUCCEEDED(result)) {
            result = target->CreateSolidColorBrush(color, &brush);
        }
        if (SUCCEEDED(result)) {
            result = layout->SetDrawingEffect(brush.Get(), range);
        }
    }
    return result;
}

/// Captures the RichEdit's RTF, native font and immutable modern-print formatting runs.
static bool CaptureRichPrintJob(PrintJob* job, HWND edit) {
    Microsoft::WRL::ComPtr<IRichEditOle> ole;
    if (!SendMessageW(edit, EM_GETOLEINTERFACE, 0, reinterpret_cast<LPARAM>(ole.GetAddressOf()))) {
        return false;
    }
    Microsoft::WRL::ComPtr<ITextDocument> document;
    Microsoft::WRL::ComPtr<ITextRange> range;
    if (FAILED(ole.As(&document)) || FAILED(document->Range(0, 0, &range))) {
        return false;
    }
    try {
        std::vector<PrintTextRun> formats;
        LONG position = 0;
        LONG length = lstrlenW(job->report);
        while (position < length) {
            LONG count = 0;
            LONG end = 0;
            bool measured = SUCCEEDED(range->SetRange(position, position + 1))
                && SUCCEEDED(range->Expand(tomCharFormat, &count))
                && SUCCEEDED(range->GetEnd(&end));
            end = std::min(end, length);
            PrintTextRun run{};
            if (!measured || end <= position || !CapturePrintRun(range.Get(), position, end, run)) {
                return false;
            }
            formats.push_back(run);
            position = end;
        }
        PrintRtfCapture rtf;
        EDITSTREAM stream{};
        stream.dwCookie = reinterpret_cast<DWORD_PTR>(&rtf);
        stream.pfnCallback = CapturePrintRtf;
        SendMessageW(edit, EM_STREAMOUT, SF_RTF, reinterpret_cast<LPARAM>(&stream));
        if (stream.dwError
                || rtf.bytes.empty()
                || rtf.bytes.size() > MAXDWORD
                || formats.size() > MAXDWORD / sizeof(PrintTextRun)) {
            return false;
        }
        job->richText = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, rtf.bytes.size()));
        job->formats = static_cast<PrintTextRun*>(HeapAlloc(GetProcessHeap(), 0,
            formats.size() * sizeof(PrintTextRun)));
        if (!job->richText || !job->formats) {
            return false;
        }
        memcpy(job->richText, rtf.bytes.data(), rtf.bytes.size());
        memcpy(job->formats, formats.data(), formats.size() * sizeof(PrintTextRun));
        job->richTextSize = static_cast<ULONG>(rtf.bytes.size());
        job->formatCount = static_cast<UINT>(formats.size());
        job->formatLayout = FormatPrintLayout;
        CHARFORMAT2W font{};
        font.cbSize = sizeof(font);
        SendMessageW(edit, EM_GETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&font));
        job->fontDpi = 1440;
        job->font.lfHeight = -font.yHeight;
        job->font.lfWidth = 0;
        job->font.lfWeight = font.dwEffects & CFE_BOLD ? FW_BOLD : FW_NORMAL;
        job->font.lfItalic = static_cast<BYTE>((font.dwEffects & CFE_ITALIC) != 0);
        job->font.lfUnderline = static_cast<BYTE>((font.dwEffects & CFE_UNDERLINE) != 0);
        job->font.lfStrikeOut = static_cast<BYTE>((font.dwEffects & CFE_STRIKEOUT) != 0);
        wcsncpy_s(job->font.lfFaceName, font.szFaceName, _TRUNCATE);
        return true;
    } catch (const std::exception& error) {
        OutputDebugStringA("Capturing character formats for printing failed: ");
        OutputDebugStringA(error.what());
        OutputDebugStringA("\n");
        return false;
    }
}

/// Releases the printer context, captured text, rich-format buffers and process-heap print-job storage.
void FreePrintJob(PrintJob* job) {
    if (job) {
        if (job->printer) {
            DeleteDC(job->printer);
        }
        if (job->report) {
            HeapFree(GetProcessHeap(), 0, job->report);
        }
        if (job->richText) {
            HeapFree(GetProcessHeap(), 0, job->richText);
        }
        if (job->formats) {
            HeapFree(GetProcessHeap(), 0, job->formats);
        }
        HeapFree(GetProcessHeap(), 0, job);
    }
}

/// Captures text, rich formatting, font, DPI, margins and title independently of later edits.
PrintJob* CapturePrintJob(HWND owner, HWND edit, const wchar_t* title, const RECT& margins) {
    PrintJob* job = static_cast<PrintJob*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PrintJob)));
    if (!job) {
        return nullptr;
    }
    job->target = owner;
    job->fontDpi = WindowDpi(edit);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(edit, WM_GETFONT, 0, 0));
    HFONT systemFont = font ? nullptr : CreateUiFont(edit);
    if (!font) {
        font = systemFont;
    }
    job->report = CopyControlText(edit);
    bool captured = job->report && GetObjectW(font, sizeof(job->font), &job->font) == sizeof(job->font);
    if (systemFont) {
        DeleteObject(systemFont);
    }
    wchar_t className[32]{};
    GetClassNameW(edit, className, ARRAYSIZE(className));
    if (captured && !_wcsicmp(className, MSFTEDIT_CLASS)) {
        captured = CaptureRichPrintJob(job, edit);
    }
    if (!captured) {
        FreePrintJob(job);
        return nullptr;
    }
    wcsncpy_s(job->title, title, _TRUNCATE);
    job->margins = margins;
    return job;
}

/// Captures the active report and attaches the printer selected through the classic print dialog.
PrintJob* ChooseReportPrinter(HWND owner, HWND edit, const wchar_t* title,
    PrintSettings& settings, wchar_t* error, int errorCapacity) {
    error[0] = 0;
    PrintJob* job = CapturePrintJob(owner, edit, title, settings.GetMargins());
    if (!job) {
        wcsncpy_s(error, errorCapacity, T(TXT_PRINT_FAILED), _TRUNCATE);
        return nullptr;
    }
    job->printer = settings.ChoosePrinter(owner, &job->printToFile, error, errorCapacity);
    if (!job->printer) {
        FreePrintJob(job);
        return nullptr;
    }
    return job;
}

/// Paginates and prints the captured text with its font and margins while honoring cancellation.
bool PrintReportPages(PrintJob* job) {
    if (job->canceled || !job->fontDpi) {
        return false;
    }
    PrintLayout layout;
    RECT area = PrintContentRect(job->printer, job->margins);
    if (!layout.Initialize(*job, job->printer, area, GetDeviceCaps(job->printer, LOGPIXELSX),
        GetDeviceCaps(job->printer, LOGPIXELSY))) {
        return false;
    }
    DOCINFOW document = DOCINFOW{};
    document.cbSize = sizeof(document);
    document.lpszDocName = job->title;
    document.lpszOutput = job->printToFile ? L"FILE:" : nullptr;
    bool started = !job->canceled && StartDocW(job->printer, &document) > 0;
    bool success = started;
    for (UINT page = 0; page < layout.GetPageCount() && success; ++page) {
        if (job->canceled || StartPage(job->printer) <= 0) {
            success = false;
            break;
        }
        success = layout.DrawPage(job->printer, page) && EndPage(job->printer) > 0;
    }
    if (success && !job->canceled) {
        success = EndDoc(job->printer) > 0;
    } else {
        success = false;
    }
    if (!success) {
        DWORD failure = GetLastError();
        if (failure == ERROR_CANCELLED || failure == ERROR_PRINT_CANCELLED) {
            InterlockedExchange(&job->canceled, 1);
        }
        if (started) {
            AbortDoc(job->printer);
        }
    }
    return success;
}

/// Returns an owned copy of a control's current Unicode text.
std::wstring GetControlText(HWND control) {
    wchar_t* text = CopyControlText(control);
    std::wstring result = text ? text : L"";
    if (text) {
        HeapFree(GetProcessHeap(), 0, text);
    }
    return result;
}

/// Returns the requested localized control caption with its existing mnemonic markers.
std::wstring Mnemonic(AppLanguage language, TextId id) {
    return GetLocalizedText(language, id);
}

/// Copies Unicode text to the clipboard, transferring buffer ownership only after a successful copy.
bool CopyTextToClipboard(HWND owner, const std::wstring& text) {
    return CopyTextToClipboard(owner, text.c_str());
}

/// Exposes Windows Unicode whitespace classification to shared information forms.
bool IsUnicodeWhitespace(wchar_t character) {
    return IsTextWhitespace(character);
}

/// Leaves screen-saver inhibition disabled for this document viewer.
bool ShouldInhibitScreenSaver() {
    return false;
}

/// Positions an information-window child without activation or Z-order changes, enforcing positive
/// dimensions.
void PlaceInformationControl(HWND window, int id, int x, int y, int width, int height) {
    HWND control = GetDlgItem(window, id);
    int controlWidth = std::max(1, width);
    int controlHeight = std::max(1, height);
    UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
    SetWindowPos(control, nullptr, x, y, controlWidth, controlHeight, flags);
}

/// Measures a label's wrapped height at the requested width using its actual font, or returns zero if a DC is
/// unavailable.
int InformationLabelHeight(HWND control, int width) {
    HDC dc = GetDC(control);
    if (dc == nullptr) {
        return 0;
    }
    std::wstring text = GetControlText(control);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
    HGDIOBJ oldFont = font == nullptr ? nullptr : SelectObject(dc, font);
    RECT bounds = { 0, 0, std::max(1, width), 0 };
    int height = DrawWordWrappedText(dc, text, bounds, DT_CALCRECT | DT_NOPREFIX);
    if (oldFont != nullptr) {
        SelectObject(dc, oldFont);
    }
    ReleaseDC(control, dc);
    return height;
}

/// Measures native dialog base units from the supplied interface font.
static bool GetDialogFontBaseUnits(HFONT font, SIZE* units) {
    HDC dc = GetDC(nullptr);
    if (dc == nullptr) {
        return false;
    }
    HGDIOBJ previous = font == nullptr ? nullptr : SelectObject(dc, font);
    SIZE alphabet = {};
    TEXTMETRICW metrics = {};
    bool measured = GetTextExtentPoint32W(dc, L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
        52, &alphabet) && GetTextMetricsW(dc, &metrics);
    if (measured) {
        units->cx = (alphabet.cx / 26 + 1) / 2;
        units->cy = metrics.tmHeight;
    }
    if (previous != nullptr) {
        SelectObject(dc, previous);
    }
    ReleaseDC(nullptr, dc);
    return measured;
}

/// Converts current information-dialog font units to native pixel dimensions.
SIZE InformationDialogUnits(HFONT font, int x, int y) {
    SIZE units = {};
    if (!GetDialogFontBaseUnits(font, &units)) {
        return SIZE{};
    }
    return SIZE{ MulDiv(x, units.cx, 4), MulDiv(y, units.cy, 8) };
}

/// Measures the common native Close button using its current font and existing dialog-unit minimum and
/// padding.
SIZE InformationButtonSize(HWND window, HFONT font) {
    SIZE button = InformationDialogUnits(font, DIALOG_BUTTON_WIDTH_DLU, DIALOG_BUTTON_HEIGHT_DLU);
    SIZE ideal = {};
    HWND closeButton = GetDlgItem(window, ID_INFO_CLOSE);
    if (!SendMessageW(closeButton, BCM_GETIDEALSIZE, 0, reinterpret_cast<LPARAM>(&ideal))) {
        HDC dc = GetDC(closeButton);
        if (dc != nullptr) {
            HFONT controlFont = reinterpret_cast<HFONT>(SendMessageW(closeButton, WM_GETFONT, 0, 0));
            HGDIOBJ previous = controlFont == nullptr ? nullptr : SelectObject(dc, controlFont);
            std::wstring text = GetControlText(closeButton);
            RECT bounds = {};
            DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &bounds, DT_CALCRECT | DT_SINGLELINE);
            SIZE padding = InformationDialogUnits(font, 12, 0);
            ideal.cx = bounds.right + padding.cx + 2 * GetSystemMetrics(SM_CXEDGE);
            if (previous != nullptr) {
                SelectObject(dc, previous);
            }
            ReleaseDC(closeButton, dc);
        }
    }
    button.cx = std::max(button.cx, ideal.cx);
    return button;
}

/// Aligns a settings control with the reference text baseline, including an optional measured baseline
/// distance.
void AlignSettingsControl(HWND control, HWND reference, LONG baselineDistance) {
    LONG offset = ControlTextBaseline(reference) + baselineDistance - ControlTextBaseline(control);
    RECT bounds = ControlRect(control, GetParent(control));
    SetWindowPos(control, nullptr, bounds.left, bounds.top + offset, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/// Creates a nonactivating native tooltip for a child control and returns its owned window handle.
HWND CreateControlTooltip(HWND parent, HWND control, const wchar_t* text) {
    HWND tooltip = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!tooltip) {
        return nullptr;
    }
    TOOLINFOW information{ sizeof(information) };
    information.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    information.hwnd = parent;
    information.uId = reinterpret_cast<UINT_PTR>(control);
    information.lpszText = const_cast<wchar_t*>(text);
    if (!SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&information))) {
        DestroyWindow(tooltip);
        return nullptr;
    }
    return tooltip;
}

/// Obtains a native Windows default position with a hidden reference window and applies it without
/// activation.
bool PositionInformationWindowByWindowsDefault(HWND window) {
    RECT bounds{};
    if (!GetWindowRect(window, &bounds)) {
        return false;
    }
    HWND position = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED, CW_USEDEFAULT,
        CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!position) {
        return false;
    }
    bool positioned = GetWindowRect(position, &bounds) != FALSE;
    DestroyWindow(position);
    return positioned && SetWindowPos(window, nullptr, bounds.left, bounds.top, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/// Requests localized Help and About refresh through the owning main form.
void RefreshInformationWindows(HWND owner) {
    MainForm* form = reinterpret_cast<MainForm*>(GetWindowLongPtrW(owner, GWLP_USERDATA));
    if (form) {
        form->RefreshInformation();
    }
}

/// Retains the shared information-form hook without changing this viewer's window Z order.
void UpdateInformationWindowsTopmost(HWND) {}

/// Keeps Return, Escape and Tab available to native dialog navigation and removes the subclass on
/// destruction.
LRESULT CALLBACK InformationTextSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR) {
    if (message == WM_GETDLGCODE) {
        LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        if (wParam == VK_RETURN || wParam == VK_ESCAPE || wParam == VK_TAB) {
            result &= ~DLGC_WANTALLKEYS;
        }
        return result;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, InformationTextSubclassProc, subclassId);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

/// Creates a selected font at the target DPI, using the system font or Consolas for an empty selection.
HFONT CreateViewerFont(HWND window, const ViewerFont& saved, bool xmlSource) {
    if (!saved.font.lfFaceName[0] && !xmlSource) {
        return CreateSystemDialogFont(window);
    }
    UINT dpi = GetSystemDialogDpi(window);
    LOGFONTW description = saved.font;
    if (description.lfFaceName[0]) {
        UINT originalDpi = saved.dpi ? saved.dpi : 96;
        description.lfHeight = MulDiv(description.lfHeight, dpi, originalDpi);
        description.lfWidth = MulDiv(description.lfWidth, dpi, originalDpi);
    } else {
        description.lfHeight = -MulDiv(9, dpi, 72);
        description.lfCharSet = DEFAULT_CHARSET;
        description.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        description.lfQuality = CLEARTYPE_QUALITY;
        wcscpy_s(description.lfFaceName, L"Consolas");
    }
    return CreateFontIndirectW(&description);
}

/// Compares complete native font settings, including the DPI at which they were chosen.
bool EqualViewerFonts(const ViewerFont& left, const ViewerFont& right) {
    return left.dpi == right.dpi && !std::memcmp(&left.font, &right.font, sizeof(LOGFONTW));
}

/// Loads saved language, caption limits, packed option bits and Help placement from the registry.
void LoadSettings(AppSettings& settings) {
    DWORD bytes = sizeof(settings.helpWindowState);
    RegGetValueW(HKEY_CURRENT_USER, SettingsKey, L"HelpWindowState", RRF_RT_REG_BINARY, nullptr,
        &settings.helpWindowState, &bytes);
    DWORD value = 0;
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, SettingsKey, L"Options", RRF_RT_REG_DWORD, nullptr, &value, &size);
    settings.multipleInstances = (value & MultipleInstancesOption) != 0;
    settings.closeTabsOnDoubleClick = (value & CloseTabsOnDoubleClickOption) != 0;
    settings.smoothScrolling = (value & SmoothScrollingOption) != 0;
    settings.highlightMessage = (value & DisableMessageColorsOption) == 0;
    settings.highlightSource = (value & DisableSourceColorsOption) == 0;
    ViewerFont* fonts[] = { &settings.messageFont, &settings.sourceFont };
    const wchar_t* fontNames[] = { L"MessageFont", L"SourceFont" };
    for (size_t index = 0; index < ARRAYSIZE(fonts); ++index) {
        ViewerFont saved;
        size = sizeof(saved);
        LSTATUS loaded = RegGetValueW(HKEY_CURRENT_USER, SettingsKey, fontNames[index], RRF_RT_REG_BINARY,
            nullptr, &saved, &size);
        if (loaded == ERROR_SUCCESS && size == sizeof(saved) && saved.dpi && saved.dpi <= 960) {
            saved.font.lfFaceName[LF_FACESIZE - 1] = 0;
            *fonts[index] = saved;
        }
    }
    size = sizeof(value);
    LSTATUS result = RegGetValueW(HKEY_CURRENT_USER, SettingsKey, L"TabNameLength", RRF_RT_REG_DWORD,
        nullptr, &value, &size);
    if (result == ERROR_SUCCESS && value >= 8 && value <= 255) {
        settings.tabNameLength = value;
    }
    settings.titleNameLength = 40;
    size = sizeof(value);
    result = RegGetValueW(HKEY_CURRENT_USER, SettingsKey, L"TitleNameLength", RRF_RT_REG_DWORD,
        nullptr, &value, &size);
    if (result == ERROR_SUCCESS && value >= 8 && value <= 255) {
        settings.titleNameLength = value;
    }
    value = MAXDWORD;
    settings.language = -1;
    size = sizeof(value);
    result = RegGetValueW(HKEY_CURRENT_USER, SettingsKey, L"InterfaceLanguage", RRF_RT_REG_DWORD,
        nullptr, &value, &size);
    if (result == ERROR_SUCCESS && value < LANG_COUNT) {
        settings.language = static_cast<int>(value);
    }
}

/// Commits language, caption limits and controlled option bits under a named mutex, preserving other bits.
bool SaveSettings(const AppSettings& settings) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"FortSoft.ZfoViewer.Options");
    if (!mutex) {
        return false;
    }
    DWORD wait = WaitForSingleObject(mutex, INFINITE);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) {
        CloseHandle(mutex);
        return false;
    }
    HKEY key = nullptr;
    LSTATUS result = RegCreateKeyExW(HKEY_CURRENT_USER, SettingsKey, 0, nullptr, 0,
        KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &key, nullptr);
    if (result == ERROR_SUCCESS) {
        DWORD options = 0;
        DWORD size = sizeof(options);
        RegGetValueW(key, nullptr, L"Options", RRF_RT_REG_DWORD, nullptr, &options, &size);
        options = options & ~(MultipleInstancesOption | CloseTabsOnDoubleClickOption
                | SmoothScrollingOption | DisableMessageColorsOption | DisableSourceColorsOption)
            | (settings.multipleInstances ? MultipleInstancesOption : 0)
            | (settings.closeTabsOnDoubleClick ? CloseTabsOnDoubleClickOption : 0)
            | (settings.smoothScrolling ? SmoothScrollingOption : 0)
            | (settings.highlightMessage ? 0 : DisableMessageColorsOption)
            | (settings.highlightSource ? 0 : DisableSourceColorsOption);
        result = RegSetValueExW(key, L"Options", 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&options), sizeof(options));
        if (result == ERROR_SUCCESS) {
            DWORD value = static_cast<DWORD>(settings.language);
            result = RegSetValueExW(key, L"InterfaceLanguage", 0, REG_DWORD,
                reinterpret_cast<const BYTE*>(&value), sizeof(value));
        }
        if (result == ERROR_SUCCESS) {
            result = RegSetValueExW(key, L"TabNameLength", 0, REG_DWORD,
                reinterpret_cast<const BYTE*>(&settings.tabNameLength), sizeof(settings.tabNameLength));
        }
        if (result == ERROR_SUCCESS) {
            result = RegSetValueExW(key, L"TitleNameLength", 0, REG_DWORD,
                reinterpret_cast<const BYTE*>(&settings.titleNameLength), sizeof(settings.titleNameLength));
        }
        const ViewerFont* fonts[] = { &settings.messageFont, &settings.sourceFont };
        const wchar_t* fontNames[] = { L"MessageFont", L"SourceFont" };
        for (size_t index = 0; index < ARRAYSIZE(fonts) && result == ERROR_SUCCESS; ++index) {
            result = RegSetValueExW(key, fontNames[index], 0, REG_BINARY,
                reinterpret_cast<const BYTE*>(fonts[index]), sizeof(ViewerFont));
        }
        if (result == ERROR_SUCCESS) {
            RegDeleteValueW(key, L"MultipleInstances");
            RegDeleteValueW(key, L"CloseTabsOnDoubleClick");
        }
        RegCloseKey(key);
    }
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return result == ERROR_SUCCESS;
}

/// Persists Help placement only; About placement stays in memory for the running instance.
void SaveWindowPositions(const AppSettings& settings) {
    HKEY key = nullptr;
    const LSTATUS result = RegCreateKeyExW(HKEY_CURRENT_USER, SettingsKey, 0, nullptr, 0, KEY_SET_VALUE,
        nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS) {
        return;
    }
    RegSetValueExW(key, L"HelpWindowState", 0, REG_BINARY,
        reinterpret_cast<const BYTE*>(&settings.helpWindowState), sizeof(settings.helpWindowState));
    RegCloseKey(key);
}

/// Reads a validated, double-null-terminated recent-file list and returns its length in UTF-16 units.
DWORD LoadRecentFilesSetting(wchar_t* paths, DWORD capacity) {
    DWORD bytes = capacity * sizeof(wchar_t);
    const LSTATUS result = RegGetValueW(HKEY_CURRENT_USER, SettingsKey, L"RecentFiles", RRF_RT_REG_MULTI_SZ,
        nullptr, paths, &bytes);
    if (result != ERROR_SUCCESS || bytes < 2 * sizeof(wchar_t) || bytes % sizeof(wchar_t)) {
        return 0;
    }
    DWORD length = bytes / sizeof(wchar_t);
    return paths[length - 1] || paths[length - 2] ? 0 : length;
}

/// Stores the double-null-terminated recent-file list as a registry multi-string value.
bool SaveRecentFilesSetting(const wchar_t* paths, DWORD length) {
    HKEY key = nullptr;
    LSTATUS result = RegCreateKeyExW(HKEY_CURRENT_USER, SettingsKey, 0, nullptr, 0, KEY_SET_VALUE,
        nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    result = RegSetValueExW(key, L"RecentFiles", 0, REG_MULTI_SZ,
        reinterpret_cast<const BYTE*>(paths), length * sizeof(wchar_t));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

/// Returns the saved export-format identifier, defaulting to UTF-8 text when it is absent or invalid.
DWORD LoadExportType() {
    DWORD type = 2;
    DWORD size = sizeof(type);
    const LSTATUS result = RegGetValueW(HKEY_CURRENT_USER, SettingsKey, L"ExportFormat", RRF_RT_REG_DWORD,
        nullptr, &type, &size);
    if (result != ERROR_SUCCESS || type < 1 || type > ARRAYSIZE(exportChoices)) {
        type = 2;
    }
    return type;
}

/// Remembers the selected export format for the next export dialog.
void SaveExportType(DWORD type) {
    HKEY key = nullptr;
    const LSTATUS result = RegCreateKeyExW(HKEY_CURRENT_USER, SettingsKey, 0, nullptr, 0, KEY_SET_VALUE,
        nullptr, &key, nullptr);
    if (result == ERROR_SUCCESS) {
        RegSetValueExW(key, L"ExportFormat", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&type),
            sizeof(type));
        RegCloseKey(key);
    }
}

/// Creates and runs the main window, destroying it before the caller uninitializes COM.
static int RunMainWindow(HINSTANCE instance, AppSettings& settings, SingleInstance& single,
        const std::vector<std::filesystem::path>& files) {
    int result = 1;
    MainForm form(settings);
    if (form.Create(instance)) {
        single.ReleaseStartup();
        for (const std::filesystem::path& file : files) {
            form.OpenFile(file);
        }
        form.Show();
        UpdateWindow(form.GetWindow());
        MSG message{};
        BOOL received = 0;
        while ((received = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            if (!form.Translate(message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        result = received < 0 ? 1 : static_cast<int>(message.wParam);
    }
    return result;
}

/// Loads settings, forwards single-instance requests and runs the COM-initialized native application message
/// loop.
int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    AppSettings settings;
    LoadSettings(settings);
    AppLanguage language = settings.language < 0
        ? ApplicationLanguageFromLangId(GetUserDefaultUILanguage())
        : static_cast<AppLanguage>(settings.language);
    SetApplicationLanguage(language);
    int count = 0;
    PWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    std::vector<std::filesystem::path> files;
    for (int index = 1; arguments && index < count; ++index) {
        files.push_back(AbsolutePath(arguments[index]));
    }
    LocalFree(arguments);
    SingleInstance single(L"Local\\FortSoft.ZfoViewer.Instance");
    if (!single.IsLocked()) {
        MessageBoxW(nullptr, GetLocalizedText(language, "instanceFailed"), L"ZFO Viewer", MB_ICONERROR);
        return 1;
    }
    if (!settings.multipleInstances) {
        InstanceForwardResult forwarded = single.ForwardToExisting(files);
        if (forwarded == InstanceForwardResult::Forwarded) {
            return 0;
        }
        if (forwarded == InstanceForwardResult::Failed) {
            MessageBoxW(nullptr, GetLocalizedText(language, "forwardFailed"), L"ZFO Viewer", MB_ICONERROR);
            return 1;
        }
    }
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) {
        return 1;
    }
    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_WIN95_CLASSES | ICC_LINK_CLASS };
    InitCommonControlsEx(&controls);
    int result = RunMainWindow(instance, settings, single, files);
    CoUninitialize();
    return result;
}


/// Shortens text in the middle while retaining both ends, an optional complete extension and valid surrogate
/// pairs.
static void FormatMiddleCaption(const wchar_t* name, UINT limit, bool keepExtension, wchar_t* caption,
        DWORD capacity) {
    if (!capacity) {
        return;
    }
    UINT length = static_cast<UINT>(lstrlenW(name));
    if (length <= limit) {
        wcsncpy_s(caption, capacity, name, _TRUNCATE);
        return;
    }
    const wchar_t* extension = name + length;
    if (keepExtension) {
        for (const wchar_t* cursor = name + 1; cursor < name + length; ++cursor) {
            if (*cursor == L'.') {
                extension = cursor;
            }
        }
    }
    UINT suffix = static_cast<UINT>(name + length - extension);
    UINT stem = static_cast<UINT>(extension - name);
    UINT kept = limit > suffix + 3 ? limit - suffix - 1 : 2;
    if (kept >= stem) {
        wcsncpy_s(caption, capacity, name, _TRUNCATE);
        return;
    }
    UINT left = (kept + 1) / 2;
    UINT right = kept / 2;
    if (left && name[left - 1] >= 0xD800 && name[left - 1] <= 0xDBFF) {
        if (left == 1) {
            ++left;
        } else {
            --left;
        }
    }
    if (extension[-static_cast<int>(right)] >= 0xDC00 && extension[-static_cast<int>(right)] <= 0xDFFF) {
        if (right == 1) {
            ++right;
        } else {
            --right;
        }
    }
    DWORD used = 0;
    for (UINT index = 0; index < left && used + 1 < capacity; ++index) {
        caption[used++] = name[index];
    }
    if (used + 1 < capacity) {
        caption[used++] = L'\u2026';
    }
    for (const wchar_t* cursor = extension - right; *cursor && used + 1 < capacity; ++cursor) {
        caption[used++] = *cursor;
    }
    if (used && caption[used - 1] >= 0xD800 && caption[used - 1] <= 0xDBFF) {
        --used;
    }
    caption[used] = 0;
}

/// Shortens a long filename in the middle while retaining its beginning, stem ending and complete extension.
void FormatFileNameCaption(const wchar_t* path, UINT limit, wchar_t* caption, DWORD capacity) {
    if (!capacity) {
        return;
    }
    const wchar_t* name = path;
    for (const wchar_t* cursor = path; *cursor; ++cursor) {
        if (*cursor == L'\\' || *cursor == L'/') {
            name = cursor + 1;
        }
    }
    FormatMiddleCaption(name, limit, true, caption, capacity);
}

/// Identifies either supported Windows path separator.
static bool IsPathSeparator(wchar_t value) {
    return value == L'\\' || value == L'/';
}

/// Retains the complete drive root or UNC server and share when collapsing ancestor directories.
static UINT PathRootLength(const wchar_t* path) {
    if (path[0] && path[1] == L':' && IsPathSeparator(path[2])) {
        return 3;
    }
    if (IsPathSeparator(path[0]) && IsPathSeparator(path[1])) {
        const wchar_t* cursor = path + 2;
        for (int component = 0; component < 2; ++component) {
            while (*cursor && !IsPathSeparator(*cursor)) {
                ++cursor;
            }
            if (*cursor) {
                ++cursor;
            }
        }
        return static_cast<UINT>(cursor - path);
    }
    return 0;
}

/// Measures the complete candidate in the exact font already selected for the status pane.
static bool PathCaptionFits(HDC dc, const wchar_t* text, int width) {
    SIZE extent = SIZE{};
    return GetTextExtentPoint32W(dc, text, lstrlenW(text), &extent) && extent.cx <= width;
}

/// Combines a retained path prefix, omitted ancestors, parent name and filename using bounded native copies.
static void BuildPathCaption(const wchar_t* path, UINT prefix, bool omitted, const wchar_t* parent,
        const wchar_t* name, wchar_t* caption, DWORD capacity) {
    wcsncpy_s(caption, capacity, path, std::min<DWORD>(prefix, capacity - 1));
    DWORD used = static_cast<DWORD>(lstrlenW(caption));
    const wchar_t* parts[] = { omitted ? L"\u2026\\" : L"", parent, parent[0] ? L"\\" : L"", name };
    for (UINT index = 0; index < ARRAYSIZE(parts); ++index) {
        wcsncpy_s(caption + used, capacity - used, parts[index], _TRUNCATE);
        used += static_cast<DWORD>(lstrlenW(caption + used));
    }
    if (used && caption[used - 1] >= 0xD800 && caption[used - 1] <= 0xDBFF) {
        caption[--used] = 0;
    }
}

/// Fits an absolute path to its current font and width, retaining the root, parent directory and both
/// filename ends.
void FormatPathCaption(HDC dc, const wchar_t* path, int width, wchar_t* caption, DWORD capacity) {
    if (!capacity) {
        return;
    }
    caption[0] = 0;
    if (width <= 0) {
        return;
    }
    if (PathCaptionFits(dc, path, width)) {
        wcsncpy_s(caption, capacity, path, _TRUNCATE);
        return;
    }
    const wchar_t* name = path;
    for (const wchar_t* cursor = path; *cursor; ++cursor) {
        if (IsPathSeparator(*cursor)) {
            name = cursor + 1;
        }
    }
    UINT root = PathRootLength(path);
    const wchar_t* parent = name;
    if (name > path + root) {
        parent = name - 1;
        while (parent > path + root && !IsPathSeparator(parent[-1])) {
            --parent;
        }
    }
    UINT parentLength = name > parent ? static_cast<UINT>(name - parent - 1) : 0;
    UINT nameLength = static_cast<UINT>(lstrlenW(name));
    wchar_t parentCaption[MAX_PATH];
    wchar_t nameCaption[MAX_PATH];
    wchar_t candidate[MAX_PATH + 4];
    wcsncpy_s(parentCaption, parent, std::min<size_t>(parentLength, ARRAYSIZE(parentCaption) - 1));
    wcsncpy_s(nameCaption, name, _TRUNCATE);
    bool omitted = parent > path + root;
    BuildPathCaption(path, root, omitted, parentCaption, nameCaption, candidate, ARRAYSIZE(candidate));
    for (UINT limit = nameLength; !PathCaptionFits(dc, candidate, width) && limit > 3;) {
        FormatMiddleCaption(name, --limit, true, nameCaption, ARRAYSIZE(nameCaption));
        BuildPathCaption(path, root, omitted, parentCaption, nameCaption, candidate, ARRAYSIZE(candidate));
    }
    for (UINT limit = parentLength; !PathCaptionFits(dc, candidate, width) && limit > 3;) {
        wcsncpy_s(parentCaption, parent, std::min<size_t>(parentLength, ARRAYSIZE(parentCaption) - 1));
        FormatMiddleCaption(parentCaption, --limit, false, caption, capacity);
        wcsncpy_s(parentCaption, caption, _TRUNCATE);
        BuildPathCaption(path, root, omitted, parentCaption, nameCaption, candidate, ARRAYSIZE(candidate));
    }
    wcsncpy_s(caption, capacity, candidate, _TRUNCATE);
    for (const wchar_t* cursor = path + root; cursor < parent; ++cursor) {
        if (IsPathSeparator(*cursor) && cursor + 1 < parent) {
            UINT prefix = static_cast<UINT>(cursor + 1 - path);
            BuildPathCaption(path, prefix, true, parentCaption, nameCaption, candidate, ARRAYSIZE(candidate));
            if (!PathCaptionFits(dc, candidate, width)) {
                break;
            }
            wcsncpy_s(caption, capacity, candidate, _TRUNCATE);
        }
    }
}

/// Destroys every owned caption-icon pair cached for the application's information windows.
ProgramIcons::~ProgramIcons() {
    for (const IconPair& pair : icons) {
        DestroyIcon(pair.large);
        DestroyIcon(pair.smallIcon);
    }
}

/// Checks that the embedded application icon is available before creating information forms.
bool ProgramIcons::Load() {
    return LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ZFOVIEWER)) != nullptr;
}

/// Loads an owned application icon at the requested pixel dimensions.
HICON ProgramIcons::LoadSized(UINT size) const {
    return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ZFOVIEWER),
        IMAGE_ICON, size, size, 0));
}

/// Applies small and large caption icons, reusing the cached pair for the window's DPI.
void ProgramIcons::Apply(HWND window) const {
    UINT dpi = GetSystemDialogDpi(window);
    for (const IconPair& pair : icons) {
        if (pair.dpi == dpi) {
            SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(pair.large));
            SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(pair.smallIcon));
            return;
        }
    }
    UINT systemDpi = GetSystemDialogDpi(nullptr);
    HICON large = LoadSized(MulDiv(GetSystemMetrics(SM_CXICON), dpi, systemDpi));
    HICON smallIcon = LoadSized(MulDiv(GetSystemMetrics(SM_CXSMICON), dpi, systemDpi));
    icons.push_back(IconPair{ dpi, large, smallIcon });
    SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large));
    SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
}

/// Acquires the named startup mutex to serialize detection and creation of the first application window.
SingleInstance::SingleInstance(const std::wstring& name) {
    mutex = CreateMutexW(nullptr, FALSE, name.c_str());
    DWORD result = mutex ? WaitForSingleObject(mutex, INFINITE) : WAIT_FAILED;
    locked = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
}

/// Releases the startup lock if held and closes the mutex handle.
SingleInstance::~SingleInstance() {
    ReleaseStartup();
    if (mutex) {
        CloseHandle(mutex);
    }
}

/// Reports whether this instance currently owns the startup mutex.
bool SingleInstance::IsLocked() const {
    return locked;
}

/// Releases the startup mutex after forwarding or creating the receiving main window.
void SingleInstance::ReleaseStartup() {
    if (locked) {
        ReleaseMutex(mutex);
        locked = false;
    }
}

/// Forwards a double-null-terminated path list to an existing viewer using bounded synchronous IPC.
InstanceForwardResult SingleInstance::ForwardToExisting(const
        std::vector<std::filesystem::path>& files) const {
    HWND target = FindWindowW(MainWindowClass, nullptr);
    if (!target) {
        return InstanceForwardResult::NoWindow;
    }
    std::vector<wchar_t> data;
    for (const std::filesystem::path& file : files) {
        const std::wstring& path = file.native();
        data.insert(data.end(), path.begin(), path.end());
        data.push_back(L'\0');
    }
    data.push_back(L'\0');
    DWORD process = 0;
    GetWindowThreadProcessId(target, &process);
    AllowSetForegroundWindow(process);
    COPYDATASTRUCT request{
        CopyDataIdentifier,
        static_cast<DWORD>(data.size() * sizeof(wchar_t)),
        data.data()
    };
    DWORD_PTR accepted = 0;
    if (SendMessageTimeoutW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&request), SMTO_ABORTIFHUNG | SMTO_BLOCK, 10000, &accepted) && accepted) {
        return InstanceForwardResult::Forwarded;
    }
    return InstanceForwardResult::Failed;
}

/// Captures all native RichEdit content while preserving its viewport, selection visibility and redraw state.
static HBITMAP CaptureReportImage(HWND edit, int maximumWidth, int maximumHeight, BYTE** pixels, int* width,
        int* height) {
    const ReportFont font = GetReportFont(edit);
    RECT client{};
    RECT format{};
    POINT savedScroll{};
    GetClientRect(edit, &client);
    SendMessageW(edit, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&format));
    SendMessageW(edit, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&savedScroll));
    if (format.right <= format.left || format.bottom <= format.top) {
        return nullptr;
    }
    GETTEXTLENGTHEX query{ GTL_PRECISE | GTL_NUMCHARS, 1200 };
    LONG length = static_cast<LONG>(SendMessageW(edit, EM_GETTEXTLENGTHEX,
        reinterpret_cast<WPARAM>(&query), 0));
    POINTL end{};
    SendMessageW(edit, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&end), length);
    LONG rightInset = client.right - format.right;
    LONG bottomInset = client.bottom - format.bottom;
    LONGLONG measuredWidth = client.right;
    LONGLONG measuredHeight = static_cast<LONGLONG>(end.y) + savedScroll.y + font.linePixels + bottomInset;
    LONG_PTR style = GetWindowLongPtrW(edit, GWL_STYLE);
    if (style & ES_AUTOHSCROLL) {
        int lines = static_cast<int>(SendMessageW(edit, EM_GETLINECOUNT, 0, 0));
        for (int line = 0; line < lines; ++line) {
            LONG start = static_cast<LONG>(SendMessageW(edit, EM_LINEINDEX, line, 0));
            LONG count = static_cast<LONG>(SendMessageW(edit, EM_LINELENGTH, start, 0));
            POINTL position{};
            SendMessageW(edit, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&position), start + count);
            measuredWidth = std::max<LONGLONG>(measuredWidth,
                static_cast<LONGLONG>(position.x) + savedScroll.x + rightInset);
        }
    }
    if (measuredWidth < 1
            || measuredHeight < 1
            || measuredWidth > maximumWidth
            || measuredHeight > maximumHeight
            || measuredWidth * measuredHeight * 4 > 0x10000000) {
        return nullptr;
    }
    *width = static_cast<int>(measuredWidth);
    *height = static_cast<int>(measuredHeight);
    HDC screen = GetDC(edit);
    HDC dc = screen ? CreateCompatibleDC(screen) : nullptr;
    if (screen) {
        ReleaseDC(edit, screen);
    }
    if (!dc) {
        return nullptr;
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = *width;
    info.bmiHeader.biHeight = -*height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS,
        reinterpret_cast<void**>(pixels), nullptr, 0);
    if (!bitmap) {
        DeleteDC(dc);
        return nullptr;
    }
    HGDIOBJ previous = SelectObject(dc, bitmap);
    RECT bounds{ 0, 0, *width, *height };
    FillRect(dc, &bounds, GetSysColorBrush(COLOR_WINDOW));
    bool success = true;
    bool visible = (style & WS_VISIBLE) != 0;
    bool hideSelection = GetFocus() != edit && !(style & ES_NOHIDESEL);
    SendMessageW(edit, WM_SETREDRAW, FALSE, 0);
    SendMessageW(edit, EM_HIDESELECTION, TRUE, 0);
    int contentRight = *width - rightInset;
    int contentBottom = *height - bottomInset;
    for (int top = format.top; top < contentBottom && success;) {
        int bottom = contentBottom;
        for (int left = format.left; left < contentRight && success;) {
            POINT requested{ left - format.left, top - format.top };
            POINT actual{};
            SendMessageW(edit, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&requested));
            SendMessageW(edit, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&actual));
            int right = std::min<LONG>(contentRight, actual.x + format.right);
            bottom = std::min<LONG>(contentBottom, actual.y + format.bottom);
            int state = SaveDC(dc);
            if (!state
                    || right <= left
                    || bottom <= top
                    || actual.x + format.left > left
                    || actual.y + format.top > top) {
                if (state) {
                    RestoreDC(dc, state);
                }
                success = false;
                break;
            }
            SetViewportOrgEx(dc, actual.x, actual.y, nullptr);
            IntersectClipRect(dc, left - actual.x, top - actual.y, right - actual.x, bottom - actual.y);
            SendMessageW(edit, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_ERASEBKGND);
            RestoreDC(dc, state);
            left = right;
        }
        top = bottom;
    }
    SendMessageW(edit, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&savedScroll));
    SendMessageW(edit, EM_HIDESELECTION, hideSelection, 0);
    SendMessageW(edit, WM_SETREDRAW, TRUE, 0);
    if (!visible) {
        SetWindowPos(edit, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW);
    } else {
        InvalidateRect(edit, nullptr, FALSE);
    }
    GdiFlush();
    SelectObject(dc, previous);
    DeleteDC(dc);
    if (!success) {
        DeleteObject(bitmap);
        *pixels = nullptr;
        return nullptr;
    }
    size_t count = static_cast<size_t>(*width) * *height;
    for (size_t index = 0; index < count; ++index) {
        (*pixels)[index * 4 + 3] = 255;
    }
    return bitmap;
}

/// Captures native RichEdit rendering at its current layout and saves it with the shared image encoder.
bool SaveReportImage(HWND edit, const wchar_t* path, DWORD type, wchar_t* error, int errorCapacity) {
    if (type < 12 || type > 17) {
        wcsncpy_s(error, errorCapacity, T(TXT_IMAGE_FORMAT_UNSUPPORTED), _TRUNCATE);
        return false;
    }
    BYTE* pixels = nullptr;
    int width = 0;
    int height = 0;
    int maximumWidth = type == 17 ? 16383 : 16384;
    int maximumHeight = type == 17 ? 16383 : 32767;
    HBITMAP bitmap = CaptureReportImage(edit, maximumWidth, maximumHeight, &pixels, &width, &height);
    if (!bitmap) {
        wcsncpy_s(error, errorCapacity, T(TXT_REPORT_IMAGE_CREATE_FAILED), _TRUNCATE);
        return false;
    }
    bool success = false;
    try {
        success = ImageExport::Save(path, static_cast<SnapshotFormat>(type - 11), width, height,
            width * 4, pixels);
    } catch (...) {
        DeleteObject(bitmap);
        throw;
    }
    DeleteObject(bitmap);
    if (!success) {
        wcsncpy_s(error, errorCapacity, T(imageErrors[type - 12]), _TRUNCATE);
    }
    return success;
}

/// Writes the complete text-export buffer while handling partial writes.
static bool WriteTextBytes(HANDLE file, const void* source, DWORD size) {
    const BYTE* cursor = static_cast<const BYTE*>(source);
    while (size) {
        DWORD written = 0;
        if (!WriteFile(file, cursor, size, &written, nullptr) || !written) {
            return false;
        }
        cursor += written;
        size -= written;
    }
    return true;
}

/// Writes the selected text encoding while preserving characters and original line endings.
static bool WriteEncodedText(HANDLE file, const wchar_t* report, DWORD type, bool* encodingError) {
    int length = lstrlenW(report);
    if (type >= 4) {
        WORD marker = type == 4 ? 0xFFFE : 0xFEFF;
        if (!WriteTextBytes(file, &marker, sizeof(marker))) {
            return false;
        }
        if (type == 5) {
            return WriteTextBytes(file, report, length * sizeof(wchar_t));
        }
        BYTE buffer[4096]{};
        DWORD used = 0;
        for (int index = 0; index < length; ++index) {
            buffer[used++] = static_cast<BYTE>(report[index] >> 8);
            buffer[used++] = static_cast<BYTE>(report[index]);
            if (used == sizeof(buffer)) {
                if (!WriteTextBytes(file, buffer, used)) {
                    return false;
                }
                used = 0;
            }
        }
        return WriteTextBytes(file, buffer, used);
    }
    UINT codePage = type == 1 ? GetACP() : CP_UTF8;
    if (codePage == CP_UTF8) {
        for (int index = 0; index < length; ++index) {
            WORD value = report[index];
            if (value >= 0xD800 && value <= 0xDBFF) {
                if (++index == length || report[index] < 0xDC00 || report[index] > 0xDFFF) {
                    *encodingError = true;
                    return false;
                }
            } else if (value >= 0xDC00 && value <= 0xDFFF) {
                *encodingError = true;
                return false;
            }
        }
    }
    const BYTE marker[] = { 0xEF, 0xBB, 0xBF };
    if (type == 3 && !WriteTextBytes(file, marker, sizeof(marker))) {
        return false;
    }
    if (!length) {
        return true;
    }
    int size = WideCharToMultiByte(codePage, 0, report, length, nullptr, 0, nullptr, nullptr);
    BYTE* bytes = size > 0 ? static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, size)) : nullptr;
    if (!bytes) {
        return false;
    }
    bool success = WideCharToMultiByte(codePage, 0, report, length, reinterpret_cast<char*>(bytes),
        size, nullptr, nullptr) == size;
    if (success && type == 1) {
        wchar_t* restored = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), 0, length * sizeof(wchar_t)));
        if (restored) {
            int count = MultiByteToWideChar(codePage, 0,
                reinterpret_cast<const char*>(bytes), size, restored, length);
            *encodingError = count != length;
            for (int index = 0; index < length && !*encodingError; ++index) {
                *encodingError = restored[index] != report[index];
            }
            success = !*encodingError;
            HeapFree(GetProcessHeap(), 0, restored);
        } else {
            success = false;
        }
    }
    if (success) {
        success = WriteTextBytes(file, bytes, size);
    }
    HeapFree(GetProcessHeap(), 0, bytes);
    return success;
}

/// Saves the report in the selected text encoding, replacing the destination only after a complete write.
bool SaveReportText(const wchar_t* report, const wchar_t* path, DWORD type, wchar_t* error,
        int errorCapacity) {
    wchar_t folder[MAX_PATH];
    wchar_t temporary[MAX_PATH];
    wchar_t* name = nullptr;
    DWORD length = GetFullPathNameW(path, MAX_PATH, folder, &name);
    bool success = false;
    bool encodingError = false;
    if (report && type >= 1 && type <= 5 && length && length < MAX_PATH && name) {
        *name = 0;
        if (GetTempFileNameW(folder, L"MIV", 0, temporary)) {
            HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING,
                FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE) {
                success = WriteEncodedText(file, report, type, &encodingError);
                if (!CloseHandle(file)) {
                    success = false;
                }
            }
            if (success) {
                success = MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
                    != FALSE;
            }
            if (!success) {
                DeleteFileW(temporary);
            }
        }
    }
    if (!success) {
        wcsncpy_s(error, errorCapacity, T(encodingError ? TXT_TEXT_ENCODING_FAILED : TXT_TEXT_EXPORT_FAILED), _TRUNCATE);
    }
    return success;
}

/// Streams RichEdit RTF bytes to the export file and reports incomplete writes through the stream callback
/// result.
static DWORD CALLBACK RtfOutput(DWORD_PTR cookie, LPBYTE bytes, LONG count, LONG* copied) {
    DWORD written = 0;
    if (!WriteFile(reinterpret_cast<HANDLE>(cookie), bytes, count, &written, nullptr)) {
        *copied = 0;
        return GetLastError();
    }
    *copied = written;
    return written == static_cast<DWORD>(count) ? ERROR_SUCCESS : ERROR_WRITE_FAULT;
}

/// Streams RichEdit formatting to a temporary file and replaces the target only after a successful export.
bool SaveReportRtf(HWND edit, const wchar_t* path, wchar_t* error, int capacity) {
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    wchar_t temporary[MAX_PATH]{};
    if (!GetTempFileNameW(parent.c_str(), L"ZFO", 0, temporary)) {
        wcsncpy_s(error, capacity, GetLocalizedText(static_cast<AppLanguage>(GetApplicationLanguage()),
            "saveFailed"), _TRUNCATE);
        return false;
    }
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, nullptr,
        TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool success = file != INVALID_HANDLE_VALUE;
    if (success) {
        EDITSTREAM stream{};
        stream.dwCookie = reinterpret_cast<DWORD_PTR>(file);
        stream.pfnCallback = RtfOutput;
        SendMessageW(edit, EM_STREAMOUT, SF_RTF, reinterpret_cast<LPARAM>(&stream));
        success = !stream.dwError;
        success = CloseHandle(file) && success;
    }
    if (success) {
        success = MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!success) {
        DeleteFileW(temporary);
        wcsncpy_s(error, capacity,
            GetLocalizedText(static_cast<AppLanguage>(GetApplicationLanguage()), "saveFailed"), _TRUNCATE);
    }
    return success;
}

/// Resolves a filesystem path to an absolute Windows path and reports path-resolution failures.
std::filesystem::path AbsolutePath(const std::filesystem::path& path) {
    DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!length) {
        throw std::runtime_error("openFailed");
    }
    std::vector<wchar_t> buffer(length);
    DWORD copied = GetFullPathNameW(path.c_str(), length, buffer.data(), nullptr);
    if (!copied || copied >= length) {
        throw std::runtime_error("openFailed");
    }
    return std::filesystem::path(buffer.data());
}
