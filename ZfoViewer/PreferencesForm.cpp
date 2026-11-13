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

#include "PreferencesForm.h"
#include "MainForm.h"
#include "resource.h"
#include "SystemDialogTemplate.h"
#include "SelectionEdit.h"
#include <algorithm>
#include <commctrl.h>
#include <commdlg.h>
#include <utility>

/// Localized captions assigned to the preference controls in their existing order.
static constexpr std::pair<int, const char*> preferencesTextLabels[] = {
    { IDC_MESSAGE_FONT_LABEL, "messageFont" },
    { IDC_SOURCE_FONT_LABEL, "sourceFont" },
    { IDC_MESSAGE_FONT_DEFAULT, "fontDefault" },
    { IDC_SOURCE_FONT_DEFAULT, "fontDefault" },
    { IDC_HIGHLIGHT_MESSAGE, "highlightMessage" },
    { IDC_HIGHLIGHT_SOURCE, "highlightSource" },
    { IDC_LOWERCASE_EXTENSIONS, "lowercaseExtensions" },
    { IDC_PREFERENCES_LANGUAGE_LABEL, "languageLabel" },
    { IDC_TAB_NAME_LABEL, "tabNameLength" },
    { IDC_TITLE_NAME_LABEL, "titleNameLength" },
    { IDC_CLOSE_TABS_ON_DOUBLE_CLICK, "closeOnDoubleClick" },
    { ID_MULTIPLE_INSTANCES, "multipleInstances" },
    { IDC_SMOOTH_SCROLLING, "smoothScrolling" },
    { IDC_RECENT_LABEL, "recentFiles" },
    { IDC_RECENT_CLEAR, "clearRecentFiles" },
    { IDOK, "ok" },
    { IDCANCEL, "cancel" },
    { IDC_PREFERENCES_APPLY, "apply" }
};

static constexpr int preferencesButtons[] = { IDOK, IDCANCEL, IDC_PREFERENCES_APPLY };
static constexpr int preferencesButtonsRightToLeft[] = { IDC_PREFERENCES_APPLY, IDCANCEL, IDOK };

static const wchar_t preferencesControlProperty[] = L"Preferences.OriginalProc";
static const int preferencesLabelFields[][2] = {
    { IDC_MESSAGE_FONT_LABEL, IDC_MESSAGE_FONT_CHOOSE },
    { IDC_SOURCE_FONT_LABEL, IDC_SOURCE_FONT_CHOOSE },
    { IDC_PREFERENCES_LANGUAGE_LABEL, IDC_PREFERENCES_LANGUAGE },
    { IDC_TAB_NAME_LABEL, IDC_TAB_NAME_LENGTH },
    { IDC_TITLE_NAME_LABEL, IDC_TITLE_NAME_LENGTH }
};

/// Measures one Preferences label in its current font with native mnemonic processing.
static LONG PreferenceLabelWidth(HWND window, int control) {
    HWND label = GetDlgItem(window, control);
    wchar_t text[128];
    GetWindowTextW(label, text, 128);
    HDC dc = GetDC(label);
    if (!dc) {
        return 0;
    }
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(label, WM_GETFONT, 0, 0));
    HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;
    RECT bounds = RECT{};
    DrawTextW(dc, text, -1, &bounds, DT_CALCRECT | DT_SINGLELINE);
    if (previous) {
        SelectObject(dc, previous);
    }
    ReleaseDC(label, dc);
    return bounds.right;
}

/// Measures the native text baseline relative to a control window top.
static LONG PreferenceControlAscent(HWND control) {
    RECT bounds = RECT{};
    GetWindowRect(control, &bounds);
    return ControlTextBaseline(control) - bounds.top;
}

/// Moves a Preferences control to a shared baseline while retaining its size.
static void PlacePreferenceControlBaseline(HWND control, LONG baseline) {
    HWND parent = GetParent(control);
    RECT bounds = ControlRect(control, parent);
    POINT origin = POINT{};
    ClientToScreen(parent, &origin);
    LONG distance = origin.y + baseline - ControlTextBaseline(control);
    SetWindowPos(control, nullptr, bounds.left, bounds.top + distance, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/// Finds the enabled field associated with a localized Preferences label mnemonic.
static HWND PreferenceMnemonicTarget(HWND window, wchar_t key) {
    CharUpperBuffW(&key, 1);
    for (const int(&entry)[2] : preferencesLabelFields) {
        wchar_t caption[256];
        GetDlgItemTextW(window, entry[0], caption, ARRAYSIZE(caption));
        for (int index = 0; caption[index]; ++index) {
            if (caption[index] != L'&' || !caption[index + 1]) {
                continue;
            }
            wchar_t character = caption[++index];
            if (character == L'&') {
                continue;
            }
            CharUpperBuffW(&character, 1);
            HWND field = GetDlgItem(window, entry[1]);
            if (character == key && IsWindowEnabled(field) && GetWindowLongW(field, GWL_STYLE) & WS_VISIBLE) {
                return field;
            }
        }
    }
    return nullptr;
}

/// Preserves native field mnemonics when their labels are painted behind foreground controls.
static LRESULT CALLBACK PreferencesControlProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    WNDPROC original = reinterpret_cast<WNDPROC>(GetPropW(window, preferencesControlProperty));
    if (message == WM_NCDESTROY) {
        RemovePropW(window, preferencesControlProperty);
        SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
    }
    if (message == WM_GETDLGCODE) {
        LRESULT result = CallWindowProcW(original, window, message, wParam, lParam);
        const MSG* key = reinterpret_cast<const MSG*>(lParam);
        if (key) {
            if (key->message == WM_SYSCHAR
                    && PreferenceMnemonicTarget(GetParent(window), static_cast<wchar_t>(key->wParam))) {
                result |= DLGC_WANTMESSAGE;
            }
        }
        return result;
    }
    if (message == WM_SYSCHAR) {
        HWND target = PreferenceMnemonicTarget(GetParent(window), static_cast<wchar_t>(wParam));
        if (target) {
            SendMessageW(GetParent(window), WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(target), TRUE);
            return 0;
        }
    }
    return CallWindowProcW(original, window, message, wParam, lParam);
}

/// Installs label mnemonic routing without replacing the tested edit selection handler or native Tab
/// navigation.
static bool InstallPreferencesControlHandlers(HWND window) {
    for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        if (!(GetWindowLongW(child, GWL_STYLE) & WS_TABSTOP)) {
            continue;
        }
        WNDPROC original = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(child, GWLP_WNDPROC));
        if (!SetPropW(child, preferencesControlProperty, reinterpret_cast<HANDLE>(original))) {
            return false;
        }
        SetLastError(0);
        if (!SetWindowLongPtrW(child, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PreferencesControlProc))
                && GetLastError()) {
            RemovePropW(child, preferencesControlProperty);
            return false;
        }
    }
    return true;
}

/// Keeps native spins above their buddy edits and overlapping labels behind foreground inputs.
static void OrderPreferencesLabels(HWND window) {
    const int labels[] = {
        IDC_MESSAGE_FONT_LABEL,
        IDC_SOURCE_FONT_LABEL,
        IDC_PREFERENCES_LANGUAGE_LABEL,
        IDC_TITLE_NAME_LABEL,
        IDC_TAB_NAME_LABEL,
        IDC_RECENT_LABEL
    };
    const int spins[] = { IDC_TAB_NAME_SPIN, IDC_TITLE_NAME_SPIN };
    for (int id : labels) {
        SetWindowPos(GetDlgItem(window, id), HWND_BOTTOM, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    for (int id : spins) {
        SetWindowPos(GetDlgItem(window, id), HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

/// Opens modal Preferences with staged settings and placement retained only for the running instance.
void PreferencesForm::Show(HWND parent, HINSTANCE instance, AppSettings& preferences,
        PersistWindowState& state, AppLanguage selectedLanguage) {
    owner = parent;
    settings = &preferences;
    messageFont = preferences.messageFont;
    sourceFont = preferences.sourceFont;
    language = selectedLanguage;
    placement = &state;
    placement->SetSavingOptions(PersistWindowStateSavingOptions::None);
    placement->SetDetectionOptions(WindowDetectionOptions::NoDetection);
    placement->SetUseWindowsDefaultLocation(false);
    DLGTEMPLATE* definition = LoadSystemDialogTemplate(instance, IDD_PREFERENCES, parent);
    if (definition) {
        DialogBoxIndirectParamW(instance, definition, parent, DialogProc, reinterpret_cast<LPARAM>(this));
        HeapFree(GetProcessHeap(), 0, definition);
    }
}

/// Limits the dialog to its monitor work area and exposes a native scrollbar only when content overflows.
void PreferencesForm::FitHeight(int heightOfContent) {
    RECT bounds = RECT{};
    RECT client = RECT{};
    GetWindowRect(window, &bounds);
    GetClientRect(window, &client);
    RECT work = WindowWorkArea(bounds);
    int fullHeight = heightOfContent + bounds.bottom - bounds.top - client.bottom;
    int availableHeight = work.bottom - work.top;
    bool scrolling = fullHeight > availableHeight;
    LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    bool wasScrolling = (style & WS_VSCROLL) != 0;
    int width = bounds.right - bounds.left
        + (static_cast<int>(scrolling) - static_cast<int>(wasScrolling)) * GetSystemMetrics(SM_CXVSCROLL);
    int height = scrolling ? availableHeight : fullHeight;
    if (scrolling != wasScrolling) {
        SetWindowLongPtrW(window, GWL_STYLE, scrolling ? style | WS_VSCROLL : style & ~WS_VSCROLL);
    }
    int x = bounds.left;
    int y = bounds.top;
    if (x + width > work.right) {
        x = work.right - width;
    }
    if (x < work.left) {
        x = work.left;
    }
    if (y + height > work.bottom) {
        y = work.bottom - height;
    }
    if (y < work.top) {
        y = work.top;
    }
    UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | (scrolling != wasScrolling ? SWP_FRAMECHANGED : 0);
    SetWindowPos(window, nullptr, x, y, width, height, flags);
    contentHeight = scrolling ? heightOfContent : 0;
    if (scrolling) {
        GetClientRect(window, &client);
        SCROLLINFO information = SCROLLINFO{};
        information.cbSize = sizeof(information);
        information.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        information.nMin = 0;
        information.nMax = heightOfContent - 1;
        information.nPage = client.bottom;
        information.nPos = 0;
        SetScrollInfo(window, SB_VERT, &information, TRUE);
    }
}

/// Scrolls the dialog's existing child controls within the native vertical scrollbar range.
bool PreferencesForm::Scroll(int request) {
    if (!contentHeight) {
        return false;
    }
    SCROLLINFO information = SCROLLINFO{};
    information.cbSize = sizeof(information);
    information.fMask = SIF_ALL;
    if (!GetScrollInfo(window, SB_VERT, &information)) {
        return false;
    }
    int position = information.nPos;
    int line = DialogUnits(font, 0, 12).cy;
    if (line < 1) {
        line = 1;
    }
    switch (request) {
        case SB_LINEUP:
            position -= line;
            break;
        case SB_LINEDOWN:
            position += line;
            break;
        case SB_PAGEUP:
            position -= information.nPage;
            break;
        case SB_PAGEDOWN:
            position += information.nPage;
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            position = information.nTrackPos;
            break;
        case SB_TOP:
            position = information.nMin;
            break;
        case SB_BOTTOM:
            position = information.nMax;
            break;
        default:
            return true;
    }
    int maximum = information.nMax - static_cast<int>(information.nPage) + 1;
    if (position < information.nMin) {
        position = information.nMin;
    }
    if (position > maximum) {
        position = maximum;
    }
    if (position == information.nPos) {
        return true;
    }
    int previous = information.nPos;
    information.fMask = SIF_POS;
    information.nPos = position;
    SetScrollInfo(window, SB_VERT, &information, TRUE);
    scrollPosition = GetScrollPos(window, SB_VERT);
    ScrollWindowEx(window, 0, previous - scrollPosition, nullptr, nullptr, nullptr, nullptr,
        SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    return true;
}

/// Applies the requested DPI-scaled width reduction once for the current window DPI.
void PreferencesForm::ResizeWidth() {
    int reduction = MulDiv(50, WindowDpi(window), 96);
    int difference = reduction - widthReduction;
    if (!difference) {
        return;
    }
    RECT bounds = RECT{};
    GetWindowRect(window, &bounds);
    SetWindowPos(window, nullptr, 0, 0, bounds.right - bounds.left - difference,
        bounds.bottom - bounds.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    widthReduction = reduction;
}

/// Applies the system font and native control measurements, then lays out compact, baseline-aligned rows.
void PreferencesForm::ApplyStyle(bool updateUi) {
    HFONT next = updateUi
        ? CreateSystemDialogFont(window)
        : reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
    if (!next) {
        return;
    }
    if (updateUi) {
        SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(next), FALSE);
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(next), TRUE);
        }
    }
    if (ownsFont) {
        DeleteObject(font);
    }
    font = next;
    ownsFont = updateUi;
    ResizeWidth();
    LayoutControls();
}

/// Arranges existing controls using their current fonts without assigning a new interface font.
void PreferencesForm::LayoutControls() {
    if (scrollPosition) {
        ScrollWindowEx(window, 0, scrollPosition, nullptr, nullptr, nullptr, nullptr,
            SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
        scrollPosition = 0;
        SetScrollPos(window, SB_VERT, 0, FALSE);
    }
    HWND combo = GetDlgItem(window, IDC_PREFERENCES_LANGUAGE);
    HWND tabNumber = GetDlgItem(window, IDC_TAB_NAME_LENGTH);
    HWND titleNumber = GetDlgItem(window, IDC_TITLE_NAME_LENGTH);
    HWND close = GetDlgItem(window, IDC_CLOSE_TABS_ON_DOUBLE_CLICK);
    HWND smooth = GetDlgItem(window, IDC_SMOOTH_SCROLLING);
    HWND multiple = GetDlgItem(window, ID_MULTIPLE_INSTANCES);
    HWND clear = GetDlgItem(window, IDC_RECENT_CLEAR);
    UpdateEditMetrics(tabNumber);
    UpdateEditMetrics(titleNumber);
    UpdateComboBoxMetrics(combo);
    RECT client{};
    GetClientRect(window, &client);
    SIZE margin = DialogUnits(font, 8, 8);
    SIZE gap = DialogUnits(font, 6, 0);
    SIZE button = buttonSize;
    const int buttons[] = { IDC_MESSAGE_FONT_DEFAULT, IDC_SOURCE_FONT_DEFAULT,
        IDC_RECENT_CLEAR, IDOK, IDCANCEL, IDC_PREFERENCES_APPLY };
    for (int id : buttons) {
        button.cx = PreferredButtonSize(GetDlgItem(window, id), button).cx;
    }
    SIZE multipleSize = PreferredButtonSize(multiple, SIZE{ 0, button.cy });
    LONG recentWidth = PreferenceLabelWidth(window, IDC_RECENT_LABEL);
    LONG requiredWidth = 2 * margin.cx + multipleSize.cx + 2 * gap.cx + recentWidth + button.cx;
    if (requiredWidth > client.right) {
        RECT bounds{};
        GetWindowRect(window, &bounds);
        SetWindowPos(window, nullptr, 0, 0, bounds.right - bounds.left + requiredWidth - client.right,
            bounds.bottom - bounds.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        client.right = requiredWidth;
    }
    LONG right = client.right - margin.cx;
    LONG rightButton = right - button.cx;
    LONG column = DialogUnits(font, 80, 0).cx;
    const int labels[] = {
        IDC_MESSAGE_FONT_LABEL,
        IDC_SOURCE_FONT_LABEL,
        IDC_PREFERENCES_LANGUAGE_LABEL,
        IDC_TAB_NAME_LABEL
    };
    for (int id : labels) {
        column = std::max(column, margin.cx + PreferenceLabelWidth(window, id) + gap.cx);
        PlaceControl(GetDlgItem(window, id), margin.cx, 0, right - margin.cx, 1);
    }
    RECT dropped{};
    SendMessageW(combo, CB_GETDROPPEDCONTROLRECT, 0, reinterpret_cast<LPARAM>(&dropped));
    PlaceControl(combo, column, 0, rightButton - gap.cx - column, dropped.bottom - dropped.top);
    RECT numberBounds = ControlRect(tabNumber, window);
    PlaceControl(tabNumber, column, 0, button.cx, numberBounds.bottom - numberBounds.top);
    PlaceControl(titleNumber, rightButton, 0, button.cx, numberBounds.bottom - numberBounds.top);
    HWND messageColors = GetDlgItem(window, IDC_HIGHLIGHT_MESSAGE);
    HWND sourceColors = GetDlgItem(window, IDC_HIGHLIGHT_SOURCE);
    HWND lowercase = GetDlgItem(window, IDC_LOWERCASE_EXTENSIONS);
    SIZE messageColorsSize = PreferredButtonSize(messageColors, SIZE{ 0, button.cy });
    SIZE sourceColorsSize = PreferredButtonSize(sourceColors, SIZE{ 0, button.cy });
    SIZE lowercaseSize = PreferredButtonSize(lowercase, SIZE{ 0, button.cy });
    LONG labelWidth = std::max(recentWidth, PreferenceLabelWidth(window, IDC_TITLE_NAME_LABEL));
    LONG colorWidth = std::max(messageColorsSize.cx, sourceColorsSize.cx);
    colorWidth = std::max(colorWidth, lowercaseSize.cx);
    LONG rightColumn = std::min(rightButton - gap.cx - labelWidth, right - colorWidth);
    PlaceControl(GetDlgItem(window, IDC_TITLE_NAME_LABEL), rightColumn, 0,
        rightButton - gap.cx - rightColumn, 1);
    SIZE closeSize = PreferredButtonSize(close, SIZE{ 0, button.cy });
    SIZE smoothSize = PreferredButtonSize(smooth, SIZE{ 0, button.cy });
    LONG checkWidth = rightColumn - gap.cx - margin.cx;
    PlaceControl(close, margin.cx, 0, checkWidth, closeSize.cy);
    PlaceControl(smooth, margin.cx, 0, checkWidth, smoothSize.cy);
    PlaceControl(messageColors, rightColumn, 0, right - rightColumn, messageColorsSize.cy);
    PlaceControl(sourceColors, rightColumn, 0, right - rightColumn, sourceColorsSize.cy);
    PlaceControl(lowercase, rightColumn, 0, right - rightColumn, lowercaseSize.cy);
    PlaceControl(multiple, margin.cx, 0, multipleSize.cx, multipleSize.cy);
    PlaceControl(clear, rightButton, 0, button.cx, button.cy);
    PlaceControl(GetDlgItem(window, IDC_RECENT_LABEL), rightColumn, 0,
        rightButton - gap.cx - rightColumn, 1);
    const int fontRows[][3] = {
        { IDC_MESSAGE_FONT_LABEL, IDC_MESSAGE_FONT_CHOOSE, IDC_MESSAGE_FONT_DEFAULT },
        { IDC_SOURCE_FONT_LABEL, IDC_SOURCE_FONT_CHOOSE, IDC_SOURCE_FONT_DEFAULT }
    };
    for (const int(&row)[3] : fontRows) {
        HWND choose = GetDlgItem(window, row[1]);
        HWND reset = GetDlgItem(window, row[2]);
        PlaceControl(choose, column, 0, rightButton - gap.cx - column, button.cy);
        PlaceControl(reset, rightButton, 0, button.cx, button.cy);
        AlignControlBaseline(GetDlgItem(window, row[0]), choose);
        AlignControlBaseline(reset, choose);
    }
    LONG buttonRight = right;
    for (int id : preferencesButtonsRightToLeft) {
        PlaceControl(GetDlgItem(window, id), buttonRight - button.cx, 0, button.cx, button.cy);
        buttonRight -= button.cx + gap.cx;
    }
    const int rows[][4] = {
        { IDC_MESSAGE_FONT_LABEL, IDC_MESSAGE_FONT_CHOOSE, IDC_MESSAGE_FONT_DEFAULT, 0 },
        { IDC_SOURCE_FONT_LABEL, IDC_SOURCE_FONT_CHOOSE, IDC_SOURCE_FONT_DEFAULT, 0 },
        { IDC_PREFERENCES_LANGUAGE_LABEL, IDC_PREFERENCES_LANGUAGE, 0, 0 },
        { IDC_TAB_NAME_LABEL, IDC_TAB_NAME_LENGTH, IDC_TITLE_NAME_LENGTH, IDC_TITLE_NAME_LABEL },
        { 0, IDC_SMOOTH_SCROLLING, IDC_HIGHLIGHT_MESSAGE, 0 },
        { 0, IDC_CLOSE_TABS_ON_DOUBLE_CLICK, IDC_HIGHLIGHT_SOURCE, 0 },
        { 0, ID_MULTIPLE_INSTANCES, IDC_LOWERCASE_EXTENSIONS, 0 },
        { IDC_RECENT_LABEL, IDC_RECENT_CLEAR, 0, 0 }
    };
    LONG above = 0;
    LONG below = 0;
    for (const int(&row)[4] : rows) {
        for (int id : row) {
            if (!id) {
                continue;
            }
            HWND control = GetDlgItem(window, id);
            RECT bounds = ControlRect(control, window);
            if (id == row[0] || id == IDC_TITLE_NAME_LABEL) {
                TEXTMETRICW metrics{};
                ReadControlMetrics(control, &metrics);
                PlaceControl(control, bounds.left, bounds.top, bounds.right - bounds.left, metrics.tmHeight);
                bounds = ControlRect(control, window);
            }
            LONG ascent = PreferenceControlAscent(control);
            LONG descent = bounds.bottom - bounds.top - ascent;
            if (ascent > above) {
                above = ascent;
            }
            if (descent > below) {
                below = descent;
            }
        }
    }
    LONG baseline = margin.cy + above;
    LONG step = above + below + DialogUnits(font, 0, 1).cy;
    for (const int(&row)[4] : rows) {
        for (int id : row) {
            if (id) {
                PlacePreferenceControlBaseline(GetDlgItem(window, id), baseline);
            }
        }
        baseline += step;
    }
    const int numbers[][2] = {
        { IDC_TAB_NAME_LENGTH, IDC_TAB_NAME_SPIN },
        { IDC_TITLE_NAME_LENGTH, IDC_TITLE_NAME_SPIN }
    };
    for (const int(&number)[2] : numbers) {
        SendMessageW(GetDlgItem(window, number[1]), UDM_SETBUDDY,
            reinterpret_cast<WPARAM>(GetDlgItem(window, number[0])), 0);
    }
    LONG buttonsTop = baseline - step + below + DialogUnits(font, 0, 6).cy;
    for (int id : preferencesButtons) {
        RECT bounds = ControlRect(GetDlgItem(window, id), window);
        SetWindowPos(GetDlgItem(window, id), nullptr, bounds.left, buttonsTop, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    FitHeight(buttonsTop + button.cy + margin.cy);
    UpdateComboBoxDropDownSize(combo);
    OrderPreferencesLabels(window);
}

/// Updates localized controls and pending language selection without committing unsaved preferences.
void PreferencesForm::RefreshLanguage() {
    updating = true;
    language = static_cast<AppLanguage>(GetApplicationLanguage());
    const wchar_t* title = GetLocalizedText(language, "preferencesTitle");
    if (GetControlText(window) != title) {
        SetWindowTextW(window, title);
    }
    for (const std::pair<int, const char*>& item : preferencesTextLabels) {
        HWND control = GetDlgItem(window, item.first);
        const wchar_t* caption = GetLocalizedText(language, item.second);
        if (GetControlText(control) != caption) {
            SetWindowTextW(control, caption);
        }
    }
    MainForm* form = reinterpret_cast<MainForm*>(GetWindowLongPtrW(owner, GWLP_USERDATA));
    EnableWindow(GetDlgItem(window, IDC_RECENT_CLEAR), form && form->HasRecentFiles());
    if (font) {
        UpdateFontButtons();
        LayoutControls();
    } else {
        ApplyStyle(false);
        UpdateFontButtons();
    }
    updating = false;
    UpdateApply();
}

/// Describes both pending fonts and enables Default only for explicit custom selections.
void PreferencesForm::UpdateFontButtons() const {
    const ViewerFont* fonts[] = { &messageFont, &sourceFont };
    const int choose[] = { IDC_MESSAGE_FONT_CHOOSE, IDC_SOURCE_FONT_CHOOSE };
    const int reset[] = { IDC_MESSAGE_FONT_DEFAULT, IDC_SOURCE_FONT_DEFAULT };
    for (size_t index = 0; index < ARRAYSIZE(fonts); ++index) {
        bool custom = fonts[index]->font.lfFaceName[0] != 0;
        std::wstring text;
        if (index == 0 && !custom) {
            text = GetLocalizedText(language, "systemDefaultFontLabel");
            text += L"…";
        } else {
            HFONT current = CreateViewerFont(window, *fonts[index], index == 1);
            LOGFONTW description{};
            int copied = current ? GetObjectW(current, sizeof(description), &description) : 0;
            if (current) {
                DeleteObject(current);
            }
            if (copied != sizeof(description)) {
                MessageBoxW(window, GetLocalizedText(language, "fontFailed"), PRODUCT_NAME, MB_ICONERROR);
                continue;
            }
            int height = description.lfHeight < 0 ? -description.lfHeight : description.lfHeight;
            int points = MulDiv(height, 72, GetSystemDialogDpi(window));
            const char* style = "fontRegular";
            if (description.lfWeight >= FW_BOLD) {
                style = description.lfItalic ? "fontBoldItalic" : "fontBold";
            } else if (description.lfItalic) {
                style = "fontItalic";
            }
            text = std::wstring(description.lfFaceName) + L", " + std::to_wstring(points) + L", "
                + GetLocalizedText(language, style) + L"…";
        }
        std::wstring caption;
        for (wchar_t character : text) {
            caption += character;
            if (character == L'&') {
                caption += L'&';
            }
        }
        HWND choice = GetDlgItem(window, choose[index]);
        if (GetControlText(choice) != caption) {
            SetWindowTextW(choice, caption.c_str());
        }
        HWND button = GetDlgItem(window, reset[index]);
        if (!custom && GetFocus() == button) {
            HWND next = GetNextDlgTabItem(window, button, FALSE);
            SendMessageW(window, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(next), TRUE);
        }
        EnableWindow(button, custom);
    }
}

/// Opens the native font dialog for the selected report or source font.
void PreferencesForm::ChooseViewerFont(bool xmlSource) {
    ViewerFont& selected = xmlSource ? sourceFont : messageFont;
    HFONT current = CreateViewerFont(window, selected, xmlSource);
    LOGFONTW description{};
    int copied = current ? GetObjectW(current, sizeof(description), &description) : 0;
    if (current) {
        DeleteObject(current);
    }
    if (copied != sizeof(description)) {
        MessageBoxW(window, GetLocalizedText(language, "fontFailed"), PRODUCT_NAME, MB_ICONERROR);
        return;
    }
    CHOOSEFONTW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpLogFont = &description;
    dialog.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
    if (ChooseFontW(&dialog)) {
        selected.font = description;
        selected.dpi = GetSystemDialogDpi(window);
        UpdateFontButtons();
    } else if (CommDlgExtendedError()) {
        MessageBoxW(window, GetLocalizedText(language, "fontFailed"), PRODUCT_NAME, MB_ICONERROR);
    }
}

/// Enables Apply only when the current controls differ from the committed settings.
void PreferencesForm::UpdateApply() {
    if (updating) {
        return;
    }
    int selected = static_cast<int>(SendDlgItemMessageW(window,
        IDC_PREFERENCES_LANGUAGE, CB_GETCURSEL, 0, 0));
    UINT length = GetDlgItemInt(window, IDC_TAB_NAME_LENGTH, nullptr, FALSE);
    UINT titleLength = GetDlgItemInt(window, IDC_TITLE_NAME_LENGTH, nullptr, FALSE);
    const bool lowercase = IsDlgButtonChecked(window, IDC_LOWERCASE_EXTENSIONS) == BST_CHECKED;
    bool changed = selected != GetApplicationLanguage()
        || length != settings->tabNameLength
        || titleLength != settings->titleNameLength
        || !EqualViewerFonts(messageFont, settings->messageFont)
        || !EqualViewerFonts(sourceFont, settings->sourceFont)
        || IsDlgButtonChecked(window, IDC_HIGHLIGHT_MESSAGE) == BST_CHECKED != settings->highlightMessage
        || IsDlgButtonChecked(window, IDC_HIGHLIGHT_SOURCE) == BST_CHECKED != settings->highlightSource
        || lowercase != settings->lowercaseExtensions
        || IsDlgButtonChecked(window, ID_MULTIPLE_INSTANCES) == BST_CHECKED != settings->multipleInstances
        || IsDlgButtonChecked(window, IDC_CLOSE_TABS_ON_DOUBLE_CLICK) == BST_CHECKED
            != settings->closeTabsOnDoubleClick
        || IsDlgButtonChecked(window, IDC_SMOOTH_SCROLLING) == BST_CHECKED != settings->smoothScrolling;
    EnableWindow(GetDlgItem(window, IDC_PREFERENCES_APPLY), changed);
}

/// Validates caption limits, saves staged preferences and synchronously notifies the owner with the previous
/// settings.
bool PreferencesForm::Apply() {
    BOOL valid = FALSE;
    UINT length = GetDlgItemInt(window, IDC_TAB_NAME_LENGTH, &valid, FALSE);
    if (!valid || length < 8 || length > 255) {
        MessageBoxW(window, GetLocalizedText(language, "tabNameLengthInvalid"), PRODUCT_NAME, MB_ICONERROR);
        SetFocus(GetDlgItem(window, IDC_TAB_NAME_LENGTH));
        return false;
    }
    UINT titleLength = GetDlgItemInt(window, IDC_TITLE_NAME_LENGTH, &valid, FALSE);
    if (!valid || titleLength < 8 || titleLength > 255) {
        MessageBoxW(window, GetLocalizedText(language, "titleNameLengthInvalid"), PRODUCT_NAME, MB_ICONERROR);
        SetFocus(GetDlgItem(window, IDC_TITLE_NAME_LENGTH));
        return false;
    }
    AppSettings next = *settings;
    next.language = static_cast<AppLanguage>(SendDlgItemMessageW(window, IDC_PREFERENCES_LANGUAGE,
        CB_GETCURSEL, 0, 0));
    next.messageFont = messageFont;
    next.sourceFont = sourceFont;
    next.highlightMessage = IsDlgButtonChecked(window, IDC_HIGHLIGHT_MESSAGE) == BST_CHECKED;
    next.highlightSource = IsDlgButtonChecked(window, IDC_HIGHLIGHT_SOURCE) == BST_CHECKED;
    next.lowercaseExtensions = IsDlgButtonChecked(window, IDC_LOWERCASE_EXTENSIONS) == BST_CHECKED;
    next.tabNameLength = length;
    next.titleNameLength = titleLength;
    next.multipleInstances = IsDlgButtonChecked(window, ID_MULTIPLE_INSTANCES) == BST_CHECKED;
    next.smoothScrolling = IsDlgButtonChecked(window, IDC_SMOOTH_SCROLLING) == BST_CHECKED;
    next.closeTabsOnDoubleClick = IsDlgButtonChecked(window, IDC_CLOSE_TABS_ON_DOUBLE_CLICK) == BST_CHECKED;
    if (!SaveSettings(next)) {
        MessageBoxW(window, GetLocalizedText(language, "settingsFailed"), PRODUCT_NAME, MB_ICONERROR);
        return false;
    }
    AppSettings previous = *settings;
    bool languageChanged = next.language != GetApplicationLanguage();
    *settings = next;
    if (languageChanged) {
        SetApplicationLanguage(next.language);
        RefreshLanguage();
    }
    SendMessageW(owner, PreferencesAppliedMessage, 0, reinterpret_cast<LPARAM>(&previous));
    UpdateApply();
    return true;
}

/// Handles native dialog initialization, settings commands, scrolling, mnemonics, theme and DPI changes.
INT_PTR CALLBACK PreferencesForm::DialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    PreferencesForm* form = reinterpret_cast<PreferencesForm*>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        form = reinterpret_cast<PreferencesForm*>(lParam);
        SetWindowLongPtrW(window, DWLP_USER, reinterpret_cast<LONG_PTR>(form));
        form->window = window;
        RECT buttonBounds = ControlRect(GetDlgItem(window, IDOK), window);
        form->buttonSize = SIZE{
            buttonBounds.right - buttonBounds.left,
            buttonBounds.bottom - buttonBounds.top
        };
        form->updating = true;
        const int numbers[][2] = {
            { IDC_TAB_NAME_LENGTH, IDC_TAB_NAME_SPIN },
            { IDC_TITLE_NAME_LENGTH, IDC_TITLE_NAME_SPIN }
        };
        for (const int(&number)[2] : numbers) {
            UINT length = number[0] == IDC_TAB_NAME_LENGTH
                ? form->settings->tabNameLength
                : form->settings->titleNameLength;
            SetDlgItemInt(window, number[0], length, FALSE);
            SendDlgItemMessageW(window, number[0], EM_SETLIMITTEXT, 3, 0);
            SendDlgItemMessageW(window, number[1], UDM_SETBUDDY,
                reinterpret_cast<WPARAM>(GetDlgItem(window, number[0])), 0);
            SendDlgItemMessageW(window, number[1], UDM_SETRANGE32, 8, 255);
            SendDlgItemMessageW(window, number[1], UDM_SETPOS32, 0, length);
        }
        CheckDlgButton(window, ID_MULTIPLE_INSTANCES,
            form->settings->multipleInstances ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(window, IDC_CLOSE_TABS_ON_DOUBLE_CLICK,
            form->settings->closeTabsOnDoubleClick ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(window, IDC_SMOOTH_SCROLLING,
            form->settings->smoothScrolling ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(window, IDC_HIGHLIGHT_MESSAGE,
            form->settings->highlightMessage ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(window, IDC_HIGHLIGHT_SOURCE,
            form->settings->highlightSource ? BST_CHECKED : BST_UNCHECKED);
        UINT extensionCheck = form->settings->lowercaseExtensions ? BST_CHECKED : BST_UNCHECKED;
        CheckDlgButton(window, IDC_LOWERCASE_EXTENSIONS, extensionCheck);
        if (!InstallSelectionEditHandler(GetDlgItem(window, IDC_TAB_NAME_LENGTH))
                || !InstallSelectionEditHandler(GetDlgItem(window, IDC_TITLE_NAME_LENGTH))) {
            EndDialog(window, -1);
            return TRUE;
        }
        HWND combo = GetDlgItem(window, IDC_PREFERENCES_LANGUAGE);
        for (int index = 0; index < LANG_COUNT; ++index) {
            const wchar_t* name = GetLocalizedText(static_cast<AppLanguage>(index), "languageName");
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
        }
        SendMessageW(combo, CB_SETCURSEL, GetApplicationLanguage(), 0);
        form->RefreshLanguage();
        if (!InstallPreferencesControlHandlers(window)
                || !form->placement->Attach(window, "PreferencesWindow")) {
            EndDialog(window, -1);
        }
        return TRUE;
    }
    if (!form) {
        return FALSE;
    }
    if (message == WM_VSCROLL && !lParam && form->Scroll(LOWORD(wParam))) {
        return TRUE;
    }
    if (message == WM_MOUSEWHEEL && form->contentHeight) {
        form->wheelRemainder += static_cast<short>(HIWORD(wParam));
        int steps = form->wheelRemainder / WHEEL_DELTA;
        form->wheelRemainder %= WHEEL_DELTA;
        int count = steps > 0 ? steps : -steps;
        for (int index = 0; index < count * 3; ++index) {
            form->Scroll(steps > 0 ? SB_LINEUP : SB_LINEDOWN);
        }
        return TRUE;
    }
    if (message == WM_NCDESTROY) {
        if (form->ownsFont) {
            DeleteObject(form->font);
        }
        form->font = nullptr;
        form->ownsFont = false;
        return FALSE;
    }
    if (message == WM_DPICHANGED) {
        const RECT& bounds = *reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(window, nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
            bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
        form->ApplyStyle();
        return TRUE;
    }
    if (message == WM_SETTINGCHANGE || message == WM_THEMECHANGED || message == WM_DISPLAYCHANGE) {
        form->ApplyStyle();
        return TRUE;
    }
    if (message == WM_CLOSE || message == WM_COMMAND && LOWORD(wParam) == IDCANCEL) {
        EndDialog(window, IDCANCEL);
        return TRUE;
    }
    if (message == WM_COMMAND) {
        UINT id = LOWORD(wParam);
        if (id == IDOK || id == IDC_PREFERENCES_APPLY) {
            if (form->Apply() && id == IDOK) {
                EndDialog(window, IDOK);
            }
        } else if (id == IDC_MESSAGE_FONT_CHOOSE || id == IDC_SOURCE_FONT_CHOOSE) {
            form->ChooseViewerFont(id == IDC_SOURCE_FONT_CHOOSE);
            form->UpdateApply();
        } else if (id == IDC_MESSAGE_FONT_DEFAULT || id == IDC_SOURCE_FONT_DEFAULT) {
            ViewerFont& selected = id == IDC_SOURCE_FONT_DEFAULT ? form->sourceFont : form->messageFont;
            selected = ViewerFont{};
            form->UpdateFontButtons();
            form->UpdateApply();
        } else if (id == IDC_RECENT_CLEAR) {
            SendMessageW(form->owner, WM_COMMAND, ID_CLEAR_RECENT_FILES, 0);
            EnableWindow(GetDlgItem(window, IDC_RECENT_CLEAR), FALSE);
        } else if (id == IDC_PREFERENCES_LANGUAGE && HIWORD(wParam) == CBN_DROPDOWN) {
            UpdateComboBoxDropDownSize(GetDlgItem(window, id));
        } else {
            form->UpdateApply();
        }
        return TRUE;
    }
    return FALSE;
}
