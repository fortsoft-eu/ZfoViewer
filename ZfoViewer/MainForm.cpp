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

#include "MainForm.h"
#include "resource.h"
#include "ZfoViewer.h"
#include "PreferencesForm.h"
#include "ModernPrintDialog.h"
#include "SystemDialogTemplate.h"
#include "TabControl.h"
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <windowsx.h>
#include <wrl/client.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

/// Describes a localized menu command or separator and its top-level menu section.
struct MenuCaption {
    int menu;
    UINT id;
    const char* key;
};

/// Top-level menu captions and command rows retained while languages change.
static const char* const menuSections[] = { "file", "edit", "view", "options", "helpMenu" };
static const MenuCaption menuCaptions[] = {
    { 0, ID_FILE_OPEN, "open" },
    { 0, ID_FILE_OPEN_FOLDER, "openFolder" },
    { 0, ID_FILE_SAVE_COPY_AS, "saveCopyAs" },
    { 0, ID_FILE_CLOSE, "closeFile" },
    { 0, ID_FILE_CLOSE_ALL, "closeAll" },
    { 0, 0, nullptr },
    { 0, ID_FILE_EXPORT, "export" },
    { 0, 0, nullptr },
    { 0, ID_FILE_SHOW_IN_FOLDER, "showInFolder" },
    { 0, 0, nullptr },
    { 0, ID_FILE_PAGE_SETUP, "pageSetup" },
    { 0, ID_FILE_PRINT, "print" },
    { 0, 0, nullptr },
    { 0, ID_FILE_EXIT, "exit" },
    { 1, ID_EDIT_COPY, "copy" },
    { 1, 0, nullptr },
    { 1, ID_EDIT_SELECT_ALL, "selectAll" },
    { 1, 0, nullptr },
    { 1, ID_EDIT_FIND, "findMenu" },
    { 1, ID_EDIT_FIND_NEXT, "findNextMenu" },
    { 1, ID_EDIT_FIND_PREVIOUS, "findPreviousMenu" },
    { 1, 0, nullptr },
    { 1, ID_EDIT_COPY_FILE_NAME, "copyFileName" },
    { 1, ID_EDIT_COPY_FULL_PATH, "copyFullPath" },
    { 2, ID_VIEW_HIGHLIGHT, "colorHighlighting" },
    { 2, 0, nullptr },
    { 2, ID_VIEW_SOURCE, "xmlSource" },
    { 3, ID_FILE_PREFERENCES, "preferences" },
    { 4, ID_HELP_CONTENTS, "help" },
    { 4, 0, nullptr },
    { 4, ID_HELP_ABOUT, "about" },
};

static constexpr UINT MessageCommands[] = {
    ID_FILE_SAVE_COPY_AS,
    ID_FILE_EXPORT,
    ID_FILE_CLOSE,
    ID_FILE_CLOSE_ALL,
    ID_EDIT_COPY_FILE_NAME,
    ID_EDIT_COPY_FULL_PATH,
    ID_FILE_SHOW_IN_FOLDER
};

static constexpr UINT FindCommands[] = { ID_EDIT_FIND, ID_EDIT_FIND_NEXT, ID_EDIT_FIND_PREVIOUS };
static constexpr UINT PrintCommands[] = { ID_FILE_PRINT, ID_FILE_PAGE_SETUP };

/// Identifiers for RichEdit subclassing and asynchronous print completion.
static constexpr UINT_PTR FileDropSubclassId = 1;
static constexpr UINT PrintCompleteMessage = WM_APP + 21;

/// Leaves the hidden reference dialog's initial placement to the native dialog manager.
static INT_PTR CALLBACK InitialDialogPositionProc(HWND, UINT, WPARAM, LPARAM) {
    return FALSE;
}

/// Reports whether a nonempty edit contains text outside its current selection.
static bool CanSelectAll(HWND control, bool rich) {
    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(control, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    GETTEXTLENGTHEX request{ GTL_PRECISE | GTL_NUMCHARS, 1200 };
    LRESULT length = rich
        ? SendMessageW(control, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&request), 0)
        : GetWindowTextLengthW(control);
    return length > 0 && (start || end < static_cast<DWORD>(length));
}

/// Compares one decoded attachment byte with the corresponding byte read from its extracted file.
static bool AttachmentByteMatches(std::uint8_t expected, char actual) {
    return expected == static_cast<std::uint8_t>(actual);
}

/// Compares an extracted file with the original attachment bytes before reusing the existing copy.
static bool AttachmentMatchesFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& data) {
    std::ifstream input(path, std::ios::binary);
    return input && std::equal(data.begin(), data.end(), std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(), AttachmentByteMatches);
}

/// Handles dropped files and optional immediate wheel scrolling, then removes its subclass on destruction.
LRESULT CALLBACK MainForm::RichEditProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR referenceData) {
    MainForm* form = reinterpret_cast<MainForm*>(referenceData);
    if (message == WM_DROPFILES) {
        return SendMessageW(GetParent(control), message, wParam, lParam);
    }
    if (message == WM_MOUSEWHEEL) {
        int& remainder = form->wheelRemainder[control == form->source ? 1 : 0];
        if (!form->settings.smoothScrolling && !(GET_KEYSTATE_WPARAM(wParam) & (MK_CONTROL | MK_SHIFT))) {
            remainder += GET_WHEEL_DELTA_WPARAM(wParam);
            int steps = remainder / WHEEL_DELTA;
            remainder %= WHEEL_DELTA;
            UINT lines = 3;
            SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
            if (lines == WHEEL_PAGESCROLL) {
                int count = steps > 0 ? steps : -steps;
                for (int index = 0; index < count; ++index) {
                    SendMessageW(control, WM_VSCROLL, steps > 0 ? SB_PAGEUP : SB_PAGEDOWN, 0);
                }
            } else if (steps && lines) {
                LONGLONG requested = -static_cast<LONGLONG>(steps) * lines;
                int limit = static_cast<int>(SendMessageW(control, EM_GETLINECOUNT, 0, 0));
                SendMessageW(control, EM_LINESCROLL, 0,
                    static_cast<LPARAM>(std::clamp<LONGLONG>(requested, -limit, limit)));
            }
            return 0;
        }
        remainder = 0;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(control, RichEditProc, id);
    }
    return DefSubclassProc(control, message, wParam, lParam);
}

/// Obtains the selected filesystem path from a native file dialog and reports selection failures.
static std::filesystem::path DialogPath(IFileDialog* dialog) {
    ComPtr<IShellItem> item;
    PWSTR text = nullptr;
    if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &text))) {
        throw std::runtime_error("unexpectedError");
    }
    std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(text, CoTaskMemFree);
    return std::filesystem::path(path.get());
}

/// Initializes the viewer's interface language and loads its recent-file history.
MainForm::MainForm(AppSettings& preferences) : settings(preferences) {
    language = settings.language < 0
        ? ApplicationLanguageFromLangId(GetUserDefaultUILanguage())
        : static_cast<AppLanguage>(settings.language);
    SetApplicationLanguage(language);
    recentFiles.Load();
}

/// Stops printing and releases information forms, message data, temporary files, fonts and RichEdit.
MainForm::~MainForm() {
    find.Close();
    if (modernPrint) {
        CloseModernPrint(modernPrint);
        modernPrint = nullptr;
    }
    if (printThread) {
        InterlockedExchange(&activePrintJob->canceled, 1);
        CancelDC(activePrintJob->printer);
        WaitForSingleObject(printThread, INFINITE);
        CloseHandle(printThread);
        FreePrintJob(activePrintJob);
        printThread = nullptr;
        activePrintJob = nullptr;
    }
    MSG pending{};
    while (PeekMessageW(&pending, window, modernPrintCompleteMessage, modernPrintCompleteMessage,
            PM_REMOVE)) {
        ReleaseModernPrint(reinterpret_cast<ModernPrintDialog*>(pending.lParam));
    }
    about.Close();
    help.Close();
    if (IsWindow(window)) {
        DestroyWindow(window);
    }
    if (font) {
        DeleteObject(font);
    }
    if (messageFont) {
        DeleteObject(messageFont);
    }
    if (sourceFont) {
        DeleteObject(sourceFont);
    }
    if (richEdit) {
        FreeLibrary(richEdit);
    }
    messages.clear();
    TemporaryDirectory::Cleanup();
}

/// Returns the native main window, or null before it is created.
HWND MainForm::GetWindow() const {
    return window;
}

/// Returns the active message or XML-source tab, or null when no tab is selected.
MessageTab* MainForm::CurrentMessage() {
    if (activeIndex >= 0 && activeIndex < static_cast<int>(messages.size())) {
        return &messages[activeIndex];
    }
    return nullptr;
}

/// Creates the main window and connects its placement to persisted state and Windows default bounds.
bool MainForm::Create(HINSTANCE module) {
    TemporaryDirectory::Cleanup();
    instance = module;
    if (!icons.Load()) {
        return false;
    }
    wchar_t library[MAX_PATH]{};
    UINT length = GetSystemDirectoryW(library, ARRAYSIZE(library));
    if (!length || length + 14 >= ARRAYSIZE(library)) {
        return false;
    }
    wcscat_s(library, L"\\Msftedit.dll");
    richEdit = LoadLibraryW(library);
    if (!richEdit) {
        ReportError("richEditFailed");
        return false;
    }
    WNDCLASSEXW definition{};
    definition.cbSize = sizeof(definition);
    definition.hInstance = module;
    definition.lpfnWndProc = WindowProc;
    definition.lpszClassName = MainWindowClass;
    definition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    definition.hIcon = LoadIconW(module, MAKEINTRESOURCEW(IDI_ZFOVIEWER));
    definition.hIconSm = definition.hIcon;
    definition.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    if (!RegisterClassExW(&definition) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    window = CreateWindowExW(0, MainWindowClass, L"ZFO Viewer", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, module, this);
    if (!window) {
        return false;
    }
    placement.SetDetectionOptions(WindowDetectionOptions::NoDetection);
    placement.SetUseWindowsDefaultLocation(true);
    placement.SetUseWindowsDefaultSize(true);
    if (!placement.Attach(window, "MainWindow")) {
        DestroyWindow(window);
        return false;
    }
    searchWindow = window;
    return true;
}

/// Creates the native tabs, status bar and RichEdit controls and installs their message handlers.
void MainForm::CreateControls() {
    accelerators = LoadAcceleratorsW(instance, MAKEINTRESOURCEW(IDR_ACCELERATORS));
    tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | TCS_TOOLTIPS,
        0, 0, 1, 1, window, nullptr, instance, nullptr);
    status = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 1, 1, window, nullptr, instance, nullptr);
    source = CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VSCROLL
        | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY | ES_NOOLEDRAGDROP,
        0, 0, 1, 1, window, reinterpret_cast<HMENU>(ID_MAIN_SOURCE), instance, nullptr);
    if (!tabs
            || !status
            || !source
            || !report.Create(window, instance)
            || !InstallBorderlessTabHandler(tabs)) {
        throw std::runtime_error("unexpectedError");
    }
    DragAcceptFiles(window, TRUE);
    const HWND textControls[] = { report.GetWindow(), source };
    for (HWND control : textControls) {
        DragAcceptFiles(control, TRUE);
        if (!SetWindowSubclass(control, RichEditProc, FileDropSubclassId,
            reinterpret_cast<DWORD_PTR>(this))) {
            throw std::runtime_error("unexpectedError");
        }
    }
    SendMessageW(source, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
    SendMessageW(source, EM_SETEVENTMASK, 0, ENM_SELCHANGE);
    SendMessageW(source, EM_AUTOURLDETECT, FALSE, 0);
    SendMessageW(source, EM_SETBKGNDCOLOR, TRUE, 0);
    SendMessageW(source, EM_SETUNDOLIMIT, 0, 0);
    SetFonts();
    RefreshLanguage();
}

/// Applies the system dialog font and selects native RichEdit borders according to actual visual-style state.
void MainForm::SetFonts() {
    HFONT next = CreateSystemDialogFont(window);
    if (!next) {
        throw std::runtime_error("unexpectedError");
    }
    const HWND chromeControls[] = { tabs, status };
    for (HWND control : chromeControls) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(next), TRUE);
    }
    if (font) {
        DeleteObject(font);
    }
    font = next;
    SetContentFonts();
    bool themed = AreVisualStylesActive();
    const HWND textControls[] = { report.GetWindow(), source };
    for (HWND control : textControls) {
        LONG_PTR style = GetWindowLongPtrW(control, GWL_EXSTYLE);
        SetWindowLongPtrW(control, GWL_EXSTYLE,
            themed ? style & ~WS_EX_CLIENTEDGE : style | WS_EX_CLIENTEDGE);
        SetWindowPos(control, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    icons.Apply(window);
    Populate();
    Layout();
}

/// Applies independent content fonts without changing the application or information-dialog fonts.
void MainForm::SetContentFonts() {
    HFONT nextMessage = CreateViewerFont(window, settings.messageFont, false);
    HFONT nextSource = CreateViewerFont(window, settings.sourceFont, true);
    if (!nextMessage || !nextSource) {
        if (nextMessage) {
            DeleteObject(nextMessage);
        }
        if (nextSource) {
            DeleteObject(nextSource);
        }
        throw std::runtime_error("fontFailed");
    }
    SendMessageW(report.GetWindow(), WM_SETFONT, reinterpret_cast<WPARAM>(nextMessage), FALSE);
    SendMessageW(source, WM_SETFONT, reinterpret_cast<WPARAM>(nextSource), FALSE);
    if (messageFont) {
        DeleteObject(messageFont);
    }
    if (sourceFont) {
        DeleteObject(sourceFont);
    }
    messageFont = nextMessage;
    sourceFont = nextSource;
}

/// Creates menus once, then updates their localized captions, recent files and command availability in place.
void MainForm::BuildMenu() {
    HMENU menu = GetMenu(window);
    if (!menu) {
        menu = CreateMenu();
        for (const char* key : menuSections) {
            HMENU section = CreatePopupMenu();
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(section), GetLocalizedText(language, key));
        }
        for (const MenuCaption& caption : menuCaptions) {
            HMENU section = GetSubMenu(menu, caption.menu);
            AppendMenuW(section, caption.id ? MF_STRING : MF_SEPARATOR, caption.id,
                caption.id ? GetLocalizedText(language, caption.key) : nullptr);
        }
        SetMenu(window, menu);
    } else {
        for (int index = 0; index < static_cast<int>(ARRAYSIZE(menuSections)); ++index) {
            ModifyMenuW(menu, index, MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(GetSubMenu(menu,
                index)), GetLocalizedText(language, menuSections[index]));
        }
        for (const MenuCaption& caption : menuCaptions) {
            if (caption.id) {
                ModifyMenuW(GetSubMenu(menu, caption.menu), caption.id, MF_BYCOMMAND | MF_STRING, caption.id,
                    GetLocalizedText(language, caption.key));
            }
        }
    }
    recentFiles.UpdateMenu(menu);
    UpdateCommands();
    DrawMenuBar(window);
}

/// Refreshes menus, captions and information forms while preserving the active document's selection and
/// scroll.
void MainForm::RefreshLanguage() {
    CaptureViewState();
    BuildMenu();
    MessageTab* current = CurrentMessage();
    if (current && !current->sourceView) {
        HWND control = report.GetWindow();
        report.Display(&current->document, language, settings.highlightMessage);
        SendMessageW(control, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&current->selection));
        SendMessageW(control, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&current->scroll));
    }
    UpdateTabNames();
    UpdateWindowTitle();
    RefreshInformation();
    find.RefreshLanguage();
    UpdateCommands();
    Layout();
}

/// Updates the window title from the active tab using its independent name-length limit.
void MainForm::UpdateWindowTitle() {
    MessageTab* current = CurrentMessage();
    std::wstring title = GetLocalizedText(language, "applicationTitle");
    if (current) {
        title = MessageCaption(*current, settings.titleNameLength) + L" — " + title;
    }
    SetWindowTextW(window, title.c_str());
}

/// Shortens a complete filename caption, including the localized XML-source prefix when applicable.
std::wstring MainForm::MessageCaption(const MessageTab& message, UINT limit) const {
    std::wstring name = message.path.filename().wstring();
    if (message.sourceView) {
        name.insert(0, GetLocalizedText(language, "sourcePrefix"));
    }
    wchar_t caption[MAX_PATH]{};
    FormatFileNameCaption(name.c_str(), limit, caption, ARRAYSIZE(caption));
    return caption;
}

/// Displays the active message or formatted XML and restores that tab's selection and scroll position.
void MainForm::Populate() {
    wheelRemainder[0] = wheelRemainder[1] = 0;
    MessageTab* current = CurrentMessage();
    report.Display(current && !current->sourceView ? &current->document : nullptr, language,
        settings.highlightMessage);
    MessageView::DisplaySource(source, current && current->sourceView ? current->sourceText : L"",
        settings.highlightSource);
    UpdateWindowTitle();
    if (current) {
        HWND control = current->sourceView ? source : report.GetWindow();
        SendMessageW(control, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&current->selection));
        SendMessageW(control, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&current->scroll));
    }
    UpdateSearchSelection();
    UpdateCommands();
}

/// Fits the status bar and tab workspace to the client area and invalidates resized RichEdit content.
void MainForm::Layout() {
    if (!status || !tabs || !report.GetWindow()) {
        return;
    }
    RECT client{};
    RECT statusBounds{};
    GetClientRect(window, &client);
    SendMessageW(status, WM_SIZE, 0, 0);
    GetWindowRect(status, &statusBounds);
    int bottom = std::max(0L, client.bottom - statusBounds.bottom + statusBounds.top);
    MessageTab* current = CurrentMessage();
    SetWindowPos(tabs, nullptr, 0, 0, client.right, bottom,
        SWP_NOZORDER | SWP_NOACTIVATE | (current ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    RECT content = GetTabContentRect(tabs);
    const HWND textControls[] = { report.GetWindow(), source };
    for (HWND control : textControls) {
        bool visible = current && current->sourceView == (control == source);
        SetWindowPos(control, HWND_TOP, content.left, content.top,
            std::max(1L, content.right - content.left), std::max(1L, content.bottom - content.top),
            SWP_NOACTIVATE | SWP_NOCOPYBITS | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    }
    if (current) {
        HWND visible = current->sourceView ? source : report.GetWindow();
        RedrawWindow(visible, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
    }
}

/// Retains the active tab's RichEdit selection and scroll position before its view changes.
void MainForm::CaptureViewState() {
    MessageTab* current = CurrentMessage();
    if (current) {
        HWND control = current->sourceView ? source : report.GetWindow();
        SendMessageW(control, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&current->selection));
        SendMessageW(control, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&current->scroll));
    }
}

/// Saves the previous view, resets search state when needed and displays the requested tab.
void MainForm::SelectTab(int index) {
    CaptureViewState();
    if (index != activeIndex && searchWindow == window) {
        find.SetTarget(nullptr);
    }
    activeIndex = index;
    TabCtrl_SetCurSel(tabs, index);
    Populate();
    Layout();
    if (MessageTab* current = CurrentMessage()) {
        HWND control = current->sourceView ? source : report.GetWindow();
        SetFocus(control);
        SendMessageW(control, EM_HIDESELECTION, FALSE, 0);
    }
}

/// Removes one tab and its extracted files, then selects the appropriate remaining tab.
void MainForm::CloseTab(int index) {
    if (index < 0 || index >= static_cast<int>(messages.size())) {
        return;
    }
    CaptureViewState();
    int next = activeIndex;
    messages.erase(messages.begin() + index);
    TemporaryDirectory::Cleanup();
    TabCtrl_DeleteItem(tabs, index);
    if (index < next) {
        --next;
    } else if (index == next) {
        next = std::min(index, static_cast<int>(messages.size()) - 1);
    }
    activeIndex = -1;
    SelectTab(next);
}

/// Refreshes edit, search, print and file-command availability and the current status-bar text.
void MainForm::UpdateCommands() {
    UpdateFindTarget();
    HMENU menu = GetMenu(window);
    MessageTab* current = CurrentMessage();
    for (UINT id : MessageCommands) {
        EnableMenuItem(menu, id, MF_BYCOMMAND | (current ? MF_ENABLED : MF_GRAYED));
    }
    bool sourceAvailable = current && !current->sourceView && !closing;
    EnableMenuItem(menu, ID_VIEW_SOURCE, MF_BYCOMMAND | (sourceAvailable ? MF_ENABLED : MF_GRAYED));
    UINT highlightState = current && !closing ? MF_ENABLED : MF_GRAYED;
    EnableMenuItem(menu, ID_VIEW_HIGHLIGHT, MF_BYCOMMAND | highlightState);
    bool highlighted = current && current->sourceView ? settings.highlightSource : settings.highlightMessage;
    CheckMenuItem(menu, ID_VIEW_HIGHLIGHT, MF_BYCOMMAND | (highlighted ? MF_CHECKED : MF_UNCHECKED));
    HWND editTarget = TextTarget();
    EnableMenuItem(menu, ID_EDIT_COPY, MF_BYCOMMAND | (editTarget && !closing ? MF_ENABLED : MF_GRAYED));
    bool searchable = find.CanSearch();
    for (UINT id : FindCommands) {
        EnableMenuItem(menu, id, MF_BYCOMMAND | (searchable ? MF_ENABLED : MF_GRAYED));
    }
    bool canSelectAll = editTarget
        && !closing
        && CanSelectAll(editTarget, editTarget == report.GetWindow() || editTarget == source);
    EnableMenuItem(menu, ID_EDIT_SELECT_ALL, MF_BYCOMMAND | (canSelectAll ? MF_ENABLED : MF_GRAYED));
    bool printable = editTarget && !closing && !printThread && !modernPrint && !printDialogOpen;
    for (UINT id : PrintCommands) {
        EnableMenuItem(menu, id, MF_BYCOMMAND | (printable ? MF_ENABLED : MF_GRAYED));
    }
    statusText = current ? current->path.wstring() : GetLocalizedText(language, "ready");
    if (current && current->sourceView) {
        statusText.insert(0, GetLocalizedText(language, "sourcePrefix"));
    }
    SendMessageW(status, SB_SETTEXTW, SBT_OWNERDRAW, reinterpret_cast<LPARAM>(statusText.c_str()));
}

/// Displays a localized error, using the generic diagnostic when the translation key is unknown.
void MainForm::ReportError(const char* key) const {
    const wchar_t* text = GetLocalizedText(language, key);
    MessageBoxW(window, *text ? text : GetLocalizedText(language, "unexpectedError"), L"ZFO Viewer",
        MB_ICONERROR);
}

/// Opens a ZFO message or selects its existing message tab and updates recent-file history.
bool MainForm::OpenFile(const std::filesystem::path& path) {
    HCURSOR previous = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    try {
        std::filesystem::path absolute = AbsolutePath(path);
        for (int index = 0; index < static_cast<int>(messages.size()); ++index) {
            if (!messages[index].sourceView && _wcsicmp(messages[index].path.c_str(),
                absolute.c_str()) == 0) {
                SelectTab(index);
                recentFiles.Add(absolute.c_str());
                recentFiles.Save();
                BuildMenu();
                SetCursor(previous);
                return true;
            }
        }
        ZfoDocument document = ZfoDocument::Load(absolute);
        wchar_t caption[260]{};
        FormatFileNameCaption(absolute.c_str(), settings.tabNameLength, caption, ARRAYSIZE(caption));
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = caption;
        int index = static_cast<int>(messages.size());
        messages.push_back(MessageTab{ std::move(absolute), std::move(document) });
        if (TabCtrl_InsertItem(tabs, index, &item) < 0) {
            messages.pop_back();
            throw std::bad_alloc();
        }
        SelectTab(index);
        recentFiles.Add(messages[index].path.c_str());
        recentFiles.Save();
        BuildMenu();
        SetCursor(previous);
        return true;
    } catch (const std::exception& error) {
        SetCursor(previous);
        ReportError(error.what());
        return false;
    }
}

/// Shows the native picker for messages or a folder and opens the selected filesystem items.
void MainForm::OpenDialog(bool folder) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog)))) {
        throw std::runtime_error("unexpectedError");
    }
    COMDLG_FILTERSPEC filter{ GetLocalizedText(language, TXT_ZFO_FORMAT), L"*.zfo" };
    dialog->SetFileTypes(1, &filter);
    dialog->SetOptions(FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT);
    HRESULT result = dialog->Show(window);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return;
    }
    if (FAILED(result)) {
        throw std::runtime_error("unexpectedError");
    }
    if (folder) {
        OpenFolder(DialogPath(dialog.Get()));
        return;
    }
    ComPtr<IShellItemArray> files;
    DWORD count = 0;
    if (FAILED(dialog->GetResults(&files)) || FAILED(files->GetCount(&count))) {
        throw std::runtime_error("unexpectedError");
    }
    for (DWORD index = 0; index < count; ++index) {
        ComPtr<IShellItem> file;
        PWSTR text = nullptr;
        if (FAILED(files->GetItemAt(index, &file))
                || FAILED(file->GetDisplayName(SIGDN_FILESYSPATH, &text))) {
            throw std::runtime_error("unexpectedError");
        }
        std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(text, CoTaskMemFree);
        OpenFile(path.get());
    }
}

/// Shows the native Save As dialog and returns its selected path and optional export-format index.
std::filesystem::path MainForm::SaveDialog(const std::wstring& name, const wchar_t* extension,
        const COMDLG_FILTERSPEC* filters, UINT count, UINT* type, const wchar_t* title) const {
    ComPtr<IFileSaveDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog)))) {
        throw std::runtime_error("unexpectedError");
    }
    HRESULT result = dialog->SetOptions(FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT
        | FOS_NOREADONLYRETURN);
    if (FAILED(result)) {
        throw std::runtime_error("saveFailed");
    }
    if (filters) {
        HRESULT typesResult = dialog->SetFileTypes(count, filters);
        HRESULT indexResult = dialog->SetFileTypeIndex(*type);
        if (FAILED(typesResult) || FAILED(indexResult)) {
            throw std::runtime_error("saveFailed");
        }
    }
    if (extension && FAILED(dialog->SetDefaultExtension(extension))) {
        throw std::runtime_error("saveFailed");
    }
    if (FAILED(dialog->SetFileName(name.c_str())) || title && FAILED(dialog->SetTitle(title))) {
        throw std::runtime_error("saveFailed");
    }
    result = dialog->Show(window);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return {};
    }
    if (FAILED(result)) {
        throw std::runtime_error("saveFailed");
    }
    if (type && FAILED(dialog->GetFileTypeIndex(type))) {
        throw std::runtime_error("saveFailed");
    }
    return DialogPath(dialog.Get());
}

/// Saves the active tab's original ZFO bytes independently of later changes to the source file.
void MainForm::SaveCopyAs() {
    MessageTab* current = CurrentMessage();
    if (!current) {
        return;
    }
    const std::filesystem::path original = current->path;
    const COMDLG_FILTERSPEC filter{ GetLocalizedText(language, TXT_ZFO_FORMAT), L"*.zfo" };
    UINT type = 1;
    std::filesystem::path path = SaveDialog(original.filename().wstring(), L"zfo", &filter, 1, &type,
        GetLocalizedText(language, "saveCopyAsTitle"));
    if (!path.empty()) {
        ZfoDocument::Save(path, current->document.zfoBytes);
    }
}

/// Saves the requested or selected message attachment using its original decoded bytes.
void MainForm::SaveAttachment(int index) {
    MessageTab* current = CurrentMessage();
    if (index < 0) {
        index = report.SelectedAttachment();
    }
    if (!current || index < 0 || index >= static_cast<int>(current->document.attachments.size())) {
        return;
    }
    const ZfoAttachment& attachment = current->document.attachments[index];
    std::filesystem::path path = SaveDialog(attachment.name, nullptr);
    if (!path.empty()) {
        ZfoDocument::Save(path, attachment.data);
    }
}

/// Saves the decoded binary payload associated with a metadata link.
void MainForm::SaveBinaryField(int index) {
    MessageTab* current = CurrentMessage();
    if (!current || index < 0 || index >= static_cast<int>(current->document.binaryFields.size())) {
        return;
    }
    const ZfoAttachment& field = current->document.binaryFields[index];
    std::filesystem::path path = SaveDialog(field.name, nullptr);
    if (!path.empty()) {
        ZfoDocument::Save(path, field.data);
    }
}

/// Reuses or extracts an attachment in its tab's private temporary directory and opens it in its associated
/// application.
void MainForm::OpenAttachment(int index) {
    MessageTab* current = CurrentMessage();
    if (index < 0) {
        index = report.SelectedAttachment();
    }
    if (!current || index < 0 || index >= static_cast<int>(current->document.attachments.size())) {
        return;
    }
    if (!current->temporary) {
        current->temporary = std::make_unique<TemporaryDirectory>();
    }
    const ZfoAttachment& attachment = current->document.attachments[index];
    std::filesystem::path path = current->temporary->GetPath() / attachment.name;
    if (!AttachmentMatchesFile(path, attachment.data)) {
        ZfoDocument::Save(path, attachment.data);
    }
    SHELLEXECUTEINFOW operation{};
    operation.cbSize = sizeof(operation);
    operation.fMask = SEE_MASK_FLAG_NO_UI;
    operation.hwnd = window;
    operation.lpVerb = L"open";
    operation.lpFile = path.c_str();
    operation.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&operation)) {
        throw std::runtime_error("openFailed");
    }
}

/// Exports the active report as text, Office documents, RTF, web markup, formatted XML or an image.
void MainForm::ExportReport() {
    MessageTab* current = CurrentMessage();
    if (!current) {
        return;
    }
    std::vector<COMDLG_FILTERSPEC> filters;
    UINT selected = LoadExportType();
    for (const ExportChoice& choice : exportChoices) {
        filters.push_back(COMDLG_FILTERSPEC{ T(choice.label), choice.pattern });
    }
    const ExportChoice& initial = exportChoices[selected - 1];
    std::wstring name = current->path.stem().wstring() + L"." + initial.extension;
    std::filesystem::path path = SaveDialog(name, initial.extension, filters.data(),
        static_cast<UINT>(filters.size()), &selected);
    if (path.empty()) {
        return;
    }
    const ExportChoice& choice = exportChoices[selected - 1];
    HWND edit = current->sourceView ? source : report.GetWindow();
    wchar_t error[512]{};
    bool saved = false;
    if (choice.type <= 5) {
        wchar_t* text = CopyControlText(edit);
        std::wstring normalized;
        if (text) {
            for (size_t index = 0; text[index]; ++index) {
                if (text[index] == L'\r') {
                    normalized += L"\r\n";
                    if (text[index + 1] == L'\n') {
                        ++index;
                    }
                } else {
                    normalized += text[index];
                }
            }
        }
        saved = text
            && SaveReportText(normalized.c_str(), path.c_str(), choice.type, error, ARRAYSIZE(error));
        HeapFree(GetProcessHeap(), 0, text);
    } else if (choice.type == 6 || choice.type == 7) {
        const std::wstring* sourceText = current->sourceView ? &current->sourceText : nullptr;
        const std::wstring title = current->path.filename().wstring();
        const std::vector<std::uint8_t> bytes = MessageView::FormatDocument(edit,
            current->document, sourceText, title, language, choice.type == 7,
            current->sourceView ? settings.highlightSource : settings.highlightMessage);
        ZfoDocument::Save(path, bytes);
        saved = true;
    } else if (choice.type == 8) {
        saved = SaveReportRtf(edit, path.c_str(), error, ARRAYSIZE(error));
    } else if (choice.type == 9 || choice.type == 10) {
        const std::wstring* sourceText = current->sourceView ? &current->sourceText : nullptr;
        const std::wstring title = current->path.filename().wstring();
        const std::wstring html = MessageView::FormatHtml(edit,
            current->document, sourceText, title, language, choice.type == 10,
            current->sourceView ? settings.highlightSource : settings.highlightMessage);
        saved = SaveReportText(html.c_str(), path.c_str(), 2, error, ARRAYSIZE(error));
    } else if (choice.type == 11) {
        if (current->sourceView) {
            int size = WideCharToMultiByte(CP_UTF8, 0, current->sourceText.c_str(),
                static_cast<int>(current->sourceText.size()), nullptr, 0, nullptr, nullptr);
            std::vector<std::uint8_t> bytes(size);
            WideCharToMultiByte(CP_UTF8, 0, current->sourceText.c_str(),
                static_cast<int>(current->sourceText.size()),
                reinterpret_cast<char*>(bytes.data()), size, nullptr, nullptr);
            ZfoDocument::Save(path, bytes);
        } else {
            current->document.ExportXml(path);
        }
        saved = true;
    } else {
        saved = SaveReportImage(edit, path.c_str(), choice.type, error, ARRAYSIZE(error));
    }
    if (saved) {
        SaveExportType(choice.type);
    } else {
        MessageBoxW(window, *error ? error : GetLocalizedText(language, "saveFailed"), PRODUCT_NAME,
            MB_ICONERROR);
    }
}

/// Copies the selected text from the active message, XML, Help or About edit through its native control.
void MainForm::CopySelection() {
    if (HWND text = TextTarget()) {
        SendMessageW(text, WM_COPY, 0, 0);
    }
}

/// Routes shortcuts and modeless dialog navigation only to the applicable main, search or information window.
bool MainForm::Translate(MSG& message) {
    UpdateFindTarget();
    HWND activeWindow = GetActiveWindow();
    if (message.message == WM_KEYDOWN
        && activeWindow
        && (activeWindow == window
            || activeWindow == help.GetWindow()
            || activeWindow == about.GetWindow())) {
        bool control = GetKeyState(VK_CONTROL) < 0;
        if (control && message.wParam == 'F') {
            find.Show(window);
            return true;
        }
        if (message.wParam == VK_F3) {
            find.FindNext(window, GetKeyState(VK_SHIFT) < 0);
            return true;
        }
        if (control && message.wParam == 'P') {
            PrintReport();
            return true;
        }
        if (message.wParam == VK_F1) {
            SendMessageW(window, WM_COMMAND, ID_HELP_CONTENTS, 0);
            return true;
        }
        if (activeWindow == window && control && message.wParam == 'G') {
            SendMessageW(window, WM_COMMAND, ID_FILE_PREFERENCES, 0);
            return true;
        }
        if (activeWindow == window && control && message.wParam == VK_TAB && !messages.empty()) {
            int count = static_cast<int>(messages.size());
            int direction = GetKeyState(VK_SHIFT) < 0 ? -1 : 1;
            SelectTab((activeIndex + count + direction) % count);
            return true;
        }
    }
    HWND search = find.GetWindow();
    if (message.message == WM_KEYDOWN
        && GetKeyState(VK_CONTROL) < 0
        && search
        && IsChild(search, message.hwnd)
        && (message.wParam == 'A'
            || message.wParam == 'C'
            || message.wParam == 'V'
            || message.wParam == 'X'
            || message.wParam == 'Z')) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
        return true;
    }
    if (find.Translate(&message)
            || help.GetWindow() && IsDialogMessageW(help.GetWindow(), &message)
            || about.GetWindow() && IsDialogMessageW(about.GetWindow(), &message)) {
        return true;
    }
    if (message.hwnd != window && !IsChild(window, message.hwnd)) {
        return false;
    }
    return TranslateAcceleratorW(window, accelerators, &message) || IsDialogMessageW(window, &message);
}

/// Opens Preferences and releases the dialog object when the modal session ends.
void MainForm::ShowPreferences() {
    PreferencesForm preferences;
    preferences.Show(window, instance, settings, preferencesPlacement, language);
}

/// Applies the suggested DPI bounds before rebuilding the main window fonts and layout.
void MainForm::ApplyDpiBounds(const RECT& bounds) {
    SetWindowPos(window, nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
        bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
    SetFonts();
}

/// Dispatches menu commands while preserving command-specific lifetime boundaries.
LRESULT MainForm::HandleCommand(UINT command) {
    if (command >= ID_FILE_RECENT_FIRST && command < ID_FILE_RECENT_FIRST + RecentFiles::Capacity) {
        if (const wchar_t* path = recentFiles.Get(command - ID_FILE_RECENT_FIRST)) {
            OpenFile(path);
        }
        return 0;
    }
    if (command >= ID_LANGUAGE_FIRST && command < ID_LANGUAGE_FIRST + LANG_COUNT) {
        settings.language = static_cast<int>(command - ID_LANGUAGE_FIRST);
        language = settings.language < 0
            ? ApplicationLanguageFromLangId(GetUserDefaultUILanguage())
            : static_cast<AppLanguage>(settings.language);
        SetApplicationLanguage(language);
        if (!SaveSettings(settings)) {
            ReportError("settingsFailed");
        }
        RefreshLanguage();
        return 0;
    }
    switch (command) {
        case ID_FILE_OPEN:
            OpenDialog();
            break;
        case ID_FILE_OPEN_FOLDER:
            OpenDialog(true);
            break;
        case ID_FILE_SAVE_COPY_AS:
            SaveCopyAs();
            break;
        case ID_FILE_CLOSE_ALL:
            CloseAll();
            break;
        case ID_CLEAR_RECENT_FILES:
            recentFiles.Clear();
            if (!recentFiles.Save()) {
                ReportError("settingsFailed");
            }
            BuildMenu();
            break;
        case ID_EDIT_COPY_FILE_NAME:
            CopyFileName(false);
            break;
        case ID_EDIT_COPY_FULL_PATH:
            CopyFileName(true);
            break;
        case ID_FILE_EXPORT:
            ExportReport();
            break;
        case ID_FILE_SHOW_IN_FOLDER:
            ShowInFolder(activeIndex);
            break;
        case ID_VIEW_HIGHLIGHT:
            ToggleHighlighting();
            break;
        case ID_VIEW_SOURCE:
            OpenSource();
            break;
        case ID_FILE_CLOSE:
            CloseTab(activeIndex);
            break;
        case ID_FILE_PREFERENCES:
            ShowPreferences();
            break;
        case ID_FILE_EXIT:
            DestroyWindow(window);
            break;
        case ID_ATTACHMENT_OPEN:
            OpenAttachment();
            break;
        case ID_ATTACHMENT_SAVE:
            SaveAttachment();
            break;
        case ID_EDIT_COPY:
            CopySelection();
            break;
        case ID_EDIT_SELECT_ALL:
            if (HWND edit = TextTarget()) {
                SetFocus(edit);
                SendMessageW(edit, EM_SETSEL, 0, -1);
                SendMessageW(edit, EM_HIDESELECTION, FALSE, 0);
            }
            break;
        case ID_EDIT_FIND:
            UpdateFindTarget();
            find.Show(window);
            break;
        case ID_EDIT_FIND_NEXT:
            UpdateFindTarget();
            find.FindNext(window, false);
            break;
        case ID_EDIT_FIND_PREVIOUS:
            UpdateFindTarget();
            find.FindNext(window, true);
            break;
        case ID_FILE_PRINT:
            PrintReport();
            break;
        case ID_FILE_PAGE_SETUP:
            PageSetup();
            break;
        case ID_HELP_ABOUT:
            ShowAbout();
            break;
        case ID_HELP_CONTENTS:
            help.Show(window, instance, settings, icons, helpPlacement, language, true);
            UpdateFindTarget();
            break;
    }
    return 0;
}

/// Handles status, tab, tooltip and RichEdit notifications with their native return values.
LRESULT MainForm::HandleNotification(LPARAM lParam) {
    const NMHDR& notification = *reinterpret_cast<const NMHDR*>(lParam);
    if (notification.hwndFrom == status && notification.code == NM_RCLICK) {
        DWORD position = GetMessagePos();
        ShowStatusContextMenu(POINT{ GET_X_LPARAM(position), GET_Y_LPARAM(position) });
        return TRUE;
    }
    if (notification.hwndFrom == TabCtrl_GetToolTips(tabs) && notification.code == TTN_GETDISPINFOW) {
        RECT bounds{};
        GetWindowRect(window, &bounds);
        RECT work = WindowWorkArea(bounds);
        SendMessageW(notification.hwndFrom, TTM_SETMAXTIPWIDTH, 0, work.right - work.left);
        const wchar_t* text = notification.idFrom < messages.size()
            ? messages[notification.idFrom].path.c_str()
            : nullptr;
        reinterpret_cast<NMTTDISPINFOW*>(lParam)->lpszText = const_cast<wchar_t*>(text);
        return 0;
    }
    if (notification.hwndFrom == tabs && notification.code == TCN_SELCHANGE) {
        SelectTab(TabCtrl_GetCurSel(tabs));
    } else if (notification.hwndFrom == report.GetWindow() && notification.code == EN_LINK) {
        const ENLINK& event = *reinterpret_cast<const ENLINK*>(lParam);
        if (event.msg == WM_LBUTTONUP || event.msg == WM_KEYDOWN && event.wParam == VK_RETURN) {
            const AttachmentLink* link = report.FindLink(event.chrg.cpMin);
            if (link) {
                int index = link->attachment;
                if (link->binary) {
                    SaveBinaryField(index);
                } else if (link->save) {
                    SaveAttachment(index);
                } else {
                    OpenAttachment(index);
                }
                return 1;
            }
        }
    } else if (notification.hwndFrom == report.GetWindow() || notification.hwndFrom == source) {
        if (notification.code == EN_SELCHANGE) {
            UpdateCommands();
        }
    }
    return 0;
}

/// Collects dropped paths and releases the drop handle before opening the requested messages.
void MainForm::OpenDroppedFiles(WPARAM wParam) {
    HDROP drop = reinterpret_cast<HDROP>(wParam);
    UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::filesystem::path> files;
    for (UINT index = 0; index < count; ++index) {
        std::vector<wchar_t> path(DragQueryFileW(drop, index, nullptr, 0) + 1);
        DragQueryFileW(drop, index, path.data(), static_cast<UINT>(path.size()));
        files.emplace_back(path.data());
    }
    DragFinish(drop);
    for (const std::filesystem::path& file : files) {
        OpenFile(file);
    }
}

/// Validates and queues file paths received from another application instance.
LRESULT MainForm::QueueForwardedFiles(LPARAM lParam) {
    const COPYDATASTRUCT* request = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
    if (!request
            || request->dwData != CopyDataIdentifier
            || !request->lpData
            || request->cbData < sizeof(wchar_t)
            || request->cbData % sizeof(wchar_t)) {
        return FALSE;
    }
    const wchar_t* cursor = static_cast<const wchar_t*>(request->lpData);
    const wchar_t* end = cursor + request->cbData / sizeof(wchar_t);
    if (end[-1] != L'\0') {
        return FALSE;
    }
    while (cursor < end && *cursor) {
        const wchar_t* terminator = std::find(cursor, end, L'\0');
        pendingFiles.emplace_back(std::wstring(cursor, terminator));
        cursor = terminator + 1;
    }
    PostMessageW(window, OpenForwardedFilesMessage, 0, 0);
    return TRUE;
}

/// Opens queued paths before restoring and activating the existing application window.
void MainForm::OpenForwardedFiles() {
    std::vector<std::filesystem::path> files;
    files.swap(pendingFiles);
    for (const std::filesystem::path& file : files) {
        OpenFile(file);
    }
    if (IsIconic(window)) {
        ShowWindow(window, SW_RESTORE);
    }
    SetForegroundWindow(window);
}

/// Applies changed fonts, colors, language and caption limits while preserving the active view.
void MainForm::ApplyPreferences(const AppSettings& previous) {
    bool fontsChanged = !EqualViewerFonts(previous.messageFont, settings.messageFont)
        || !EqualViewerFonts(previous.sourceFont, settings.sourceFont);
    bool colorsChanged = previous.highlightMessage != settings.highlightMessage
        || previous.highlightSource != settings.highlightSource;
    if (fontsChanged || colorsChanged) {
        CaptureViewState();
        if (fontsChanged) {
            SetContentFonts();
        }
        Populate();
    }
    AppLanguage next = static_cast<AppLanguage>(settings.language);
    if (next != language) {
        language = next;
        SetApplicationLanguage(language);
        RefreshLanguage();
    } else {
        if (previous.tabNameLength != settings.tabNameLength) {
            UpdateTabNames();
        }
        if (previous.titleNameLength != settings.titleNameLength) {
            UpdateWindowTitle();
        }
    }
}

/// Paints the empty workspace while keeping BeginPaint and EndPaint in the same handler.
void MainForm::Paint() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    if (messages.empty()) {
        FillRect(dc, &paint.rcPaint, GetSysColorBrush(COLOR_APPWORKSPACE));
    }
    EndPaint(window, &paint);
}

/// Handles main-window commands, tab and RichEdit notifications, printing, placement and lifecycle messages.
LRESULT MainForm::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            CreateControls();
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                HWND focused = GetFocus();
                if (IsChild(window, focused)) {
                    lastFocusedControl = focused;
                }
            }
            break;
        case WM_SETFOCUS:
            RestoreFocus();
            return 0;
        case WM_SIZE:
            Layout();
            return 0;
        case WM_DPICHANGED:
            ApplyDpiBounds(*reinterpret_cast<const RECT*>(lParam));
            return 0;
        case WM_SYSCOLORCHANGE:
            CaptureViewState();
            Populate();
            return 0;
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            CaptureViewState();
            SetFonts();
            return 0;
        case WM_COMMAND:
            return HandleCommand(LOWORD(wParam));
        case WM_NOTIFY:
            return HandleNotification(lParam);
        case WM_DRAWITEM:
            if (reinterpret_cast<const DRAWITEMSTRUCT*>(lParam)->hwndItem == status) {
                return DrawStatus(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
            }
            break;
        case WM_CONTEXTMENU:
            ShowContextMenu(reinterpret_cast<HWND>(wParam), lParam);
            return 0;
        case WM_DROPFILES:
            OpenDroppedFiles(wParam);
            return 0;
        case WM_COPYDATA:
            return QueueForwardedFiles(lParam);
        case OpenForwardedFilesMessage:
            OpenForwardedFiles();
            return 0;
        case PreferencesAppliedMessage:
            ApplyPreferences(*reinterpret_cast<const AppSettings*>(lParam));
            return 0;
        case tabDoubleClickMessage:
            if (settings.closeTabsOnDoubleClick) {
                CloseTab(static_cast<int>(wParam));
            }
            return 1;
        case tabCloseMessage:
            CloseTab(static_cast<int>(wParam));
            return 1;
        case WM_MOUSEWHEEL:
            if (ScrollTabHeaders(window, tabs, wParam, lParam)) {
                return 0;
            }
            break;
        case WM_PAINT:
            Paint();
            return 0;
        case WM_INITMENUPOPUP:
            UpdateCommands();
            return 0;
        case helpVisibilityChangedMessage:
        case findVisibilityChangedMessage:
            UpdateSearchSelection();
            UpdateFindTarget();
            UpdateCommands();
            return 1;
        case findMatchMessage:
            if (reinterpret_cast<HWND>(wParam) == help.GetWindow()) {
                helpPlacement.SetVisible(reinterpret_cast<HWND>(lParam));
            } else if (reinterpret_cast<HWND>(wParam) == about.GetWindow()) {
                SetWindowPos(about.GetWindow(), HWND_TOP, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                SetWindowPos(reinterpret_cast<HWND>(lParam), HWND_TOP, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            } else {
                placement.SetVisible(reinterpret_cast<HWND>(lParam));
            }
            return 0;
        case PrintCompleteMessage:
            CompletePrint(reinterpret_cast<PrintJob*>(lParam));
            return 0;
        case modernPrintCompleteMessage:
            CompleteModernPrint(reinterpret_cast<ModernPrintDialog*>(lParam));
            return 0;
        case WM_CLOSE:
            closing = true;
            if (printThread) {
                InterlockedExchange(&activePrintJob->canceled, 1);
                CancelDC(activePrintJob->printer);
                UpdateCommands();
                return 0;
            }
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            find.Close();
            about.Close();
            help.Close();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

/// Displays the main window using its saved placement or Windows default bounds.
void MainForm::Show() {
    placement.Show();
}

/// Returns the active information edit or the current message or XML edit for copy and print commands.
HWND MainForm::TextTarget() {
    HWND active = GetActiveWindow();
    if (active == window || active == help.GetWindow() || active == about.GetWindow()) {
        searchWindow = active;
    }
    if (searchWindow && (searchWindow == help.GetWindow() || searchWindow == about.GetWindow())) {
        return GetDlgItem(searchWindow, ID_INFO_TEXT);
    }
    searchWindow = window;
    if (MessageTab* current = CurrentMessage()) {
        return current->sourceView ? source : report.GetWindow();
    }
    return help.GetWindow() ? GetDlgItem(help.GetWindow(), ID_INFO_TEXT) : nullptr;
}

/// Uses native owner-relative bounds on the first About opening, then retains its position within this
/// instance.
void MainForm::ShowAbout() {
    PersistWindowStateData& state = settings.aboutWindowState;
    if (!about.GetWindow() && (state.x == CW_USEDEFAULT || state.y == CW_USEDEFAULT)) {
        DLGTEMPLATE* definition = LoadSystemDialogTemplate(instance, IDD_ABOUT, window);
        if (!definition) {
            ReportError("unexpectedError");
            return;
        }
        HWND initial = CreateDialogIndirectParamW(instance, definition, window,
            InitialDialogPositionProc, 0);
        HeapFree(GetProcessHeap(), 0, definition);
        RECT bounds{};
        bool positioned = initial && GetWindowRect(initial, &bounds);
        if (initial) {
            DestroyWindow(initial);
        }
        if (!positioned) {
            ReportError("unexpectedError");
            return;
        }
        if (state.x == CW_USEDEFAULT) {
            state.x = bounds.left;
        }
        if (state.y == CW_USEDEFAULT) {
            state.y = bounds.top;
        }
    }
    about.Show(window, instance, settings, icons, aboutPlacement, language);
    UpdateFindTarget();
}

/// Refreshes the open Help and About forms in the selected interface language.
void MainForm::RefreshInformation() {
    about.Refresh(language);
    help.Refresh(language, true);
}

/// Shows the current RichEdit search selection while Find is open and restores native hiding otherwise.
void MainForm::UpdateSearchSelection() {
    MessageTab* current = CurrentMessage();
    const HWND textControls[] = { report.GetWindow(), source };
    for (HWND control : textControls) {
        bool show = find.GetWindow() && current && current->sourceView == (control == source);
        LONG_PTR style = GetWindowLongPtrW(control, GWL_STYLE);
        SetWindowLongPtrW(control, GWL_STYLE, show ? style | ES_NOHIDESEL : style & ~ES_NOHIDESEL);
        SendMessageW(control, EM_HIDESELECTION, !show && GetFocus() != control, 0);
    }
}

/// Restores focus to the previous available child control or the active message or XML edit.
void MainForm::RestoreFocus() {
    HWND target = lastFocusedControl;
    if (!IsChild(window, target) || !IsWindowVisible(target) || !IsWindowEnabled(target)) {
        target = nullptr;
        if (MessageTab* current = CurrentMessage()) {
            target = current->sourceView ? source : report.GetWindow();
        }
    }
    lastFocusedControl = target;
    if (target) {
        SetFocus(target);
        if (target == report.GetWindow() || target == source) {
            SendMessageW(target, EM_HIDESELECTION, FALSE, 0);
        }
    }
    UpdateCommands();
}

/// Selects the edit in the last active searchable form while retaining it during search-form input.
void MainForm::UpdateFindTarget() {
    HWND active = GetActiveWindow();
    if (active && (active == window || active == help.GetWindow() || active == about.GetWindow())) {
        searchWindow = active;
    }
    HWND target = nullptr;
    if (!closing) {
        if (searchWindow && (searchWindow == help.GetWindow() || searchWindow == about.GetWindow())) {
            target = GetDlgItem(searchWindow, ID_INFO_TEXT);
        } else {
            searchWindow = window;
            MessageTab* current = CurrentMessage();
            target = current ? current->sourceView ? source : report.GetWindow() : nullptr;
        }
    }
    find.SetTarget(target);
}



/// Opens native Page Setup for the active text view and reports configuration failures.
void MainForm::PageSetup() {
    if (closing || printThread || modernPrint || printDialogOpen || !TextTarget()) {
        return;
    }
    wchar_t error[512]{};
    printDialogOpen = true;
    printSettings.PageSetup(window, error, ARRAYSIZE(error));
    printDialogOpen = false;
    if (*error) {
        MessageBoxW(window, error, L"ZFO Viewer", MB_ICONERROR);
    }
    UpdateCommands();
}

/// Captures the active text view and starts the modern print dialog or the classic background print path.
void MainForm::PrintReport() {
    if (closing || printThread || modernPrint || printDialogOpen) {
        return;
    }
    HWND edit = TextTarget();
    if (!edit) {
        return;
    }
    HWND owner = GetParent(edit);
    wchar_t title[MAX_PATH]{};
    GetWindowTextW(owner, title, ARRAYSIZE(title));
    printOwner = owner;
    if (UseModernPrintDialog()) {
        PrintJob* snapshot = CapturePrintJob(window, edit, title, printSettings.GetMargins());
        modernPrint = snapshot ? CreateModernPrint(snapshot, printSettings) : nullptr;
        HRESULT result = modernPrint ? ShowModernPrint(modernPrint, owner) : E_OUTOFMEMORY;
        if (FAILED(result)) {
            CloseModernPrint(modernPrint);
            modernPrint = nullptr;
            printOwner = nullptr;
            if (result != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                ReportError("printFailed");
            }
        }
        UpdateCommands();
        return;
    }
    wchar_t error[512]{};
    printDialogOpen = true;
    PrintJob* job = ChooseReportPrinter(owner, edit, title, printSettings, error, ARRAYSIZE(error));
    printDialogOpen = false;
    printOwner = nullptr;
    if (!job) {
        if (*error) {
            MessageBoxW(owner, error, L"ZFO Viewer", MB_ICONERROR);
        }
        UpdateCommands();
        return;
    }
    job->target = window;
    activePrintJob = job;
    printThread = CreateThread(nullptr, 0, PrintDocument, job, 0, nullptr);
    if (!printThread) {
        FreePrintJob(job);
        activePrintJob = nullptr;
        ReportError("printFailed");
    }
    UpdateCommands();
}

/// Prints a captured report on the worker thread and posts completion back to the main window.
DWORD WINAPI MainForm::PrintDocument(void* parameter) {
    PrintJob* job = static_cast<PrintJob*>(parameter);
    job->success = PrintReportPages(job);
    PostMessageW(job->target, PrintCompleteMessage, 0, reinterpret_cast<LPARAM>(job));
    return 0;
}

/// Joins a completed classic print job, releases its snapshot and reports failures unless closing.
void MainForm::CompletePrint(PrintJob* job) {
    if (job != activePrintJob) {
        return;
    }
    WaitForSingleObject(printThread, INFINITE);
    CloseHandle(printThread);
    printThread = nullptr;
    bool failed = !job->success && !job->canceled;
    FreePrintJob(job);
    activePrintJob = nullptr;
    if (closing) {
        DestroyWindow(window);
    } else {
        if (failed) {
            ReportError("printFailed");
        }
        UpdateCommands();
    }
}

/// Releases a completed modern print session and refreshes print-command availability.
void MainForm::CompleteModernPrint(ModernPrintDialog* completed) {
    if (completed != modernPrint) {
        ReleaseModernPrint(completed);
        return;
    }
    bool failed = CloseModernPrint(modernPrint);
    modernPrint = nullptr;
    printOwner = nullptr;
    ReleaseModernPrint(completed);
    if (failed && !closing) {
        ReportError("printFailed");
    }
    UpdateCommands();
}

/// Associates the native window with its form object and converts uncaught failures into localized errors.
LRESULT CALLBACK MainForm::WindowProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
    MainForm* form = reinterpret_cast<MainForm*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        form = static_cast<MainForm*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        form->window = handle;
        SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(form));
    }
    if (form) {
        try {
            return form->HandleMessage(message, wParam, lParam);
        } catch (const std::exception& error) {
            form->ReportError(error.what());
            return message == WM_CREATE ? -1 : 0;
        }
    }
    return DefWindowProcW(handle, message, wParam, lParam);
}

/// Opens ZFO files directly inside the selected folder in sorted order, reusing existing message tabs.
void MainForm::OpenFolder(const std::filesystem::path& path) {
    std::vector<std::filesystem::path> paths;
    WIN32_FIND_DATAW file{};
    std::filesystem::path pattern = path / L"*.zfo";
    HANDLE search = FindFirstFileW(pattern.c_str(), &file);
    if (search == INVALID_HANDLE_VALUE) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) {
            throw std::runtime_error("openFailed");
        }
        return;
    }
    std::unique_ptr<void, decltype(&FindClose)> handle(search, FindClose);
    while (true) {
        if (!(file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            paths.push_back(path / file.cFileName);
        }
        if (!FindNextFileW(search, &file)) {
            if (GetLastError() != ERROR_NO_MORE_FILES) {
                throw std::runtime_error("openFailed");
            }
            break;
        }
    }
    std::sort(paths.begin(), paths.end());
    for (const std::filesystem::path& item : paths) {
        OpenFile(item);
    }
}

/// Closes every tab, attempts temporary-file cleanup and returns to the empty workspace.
void MainForm::CloseAll() {
    messages.clear();
    TemporaryDirectory::Cleanup();
    activeIndex = -1;
    TabCtrl_DeleteAllItems(tabs);
    Populate();
    Layout();
}

/// Copies the complete filename or full path for the requested tab, independently of caption shortening.
void MainForm::CopyFileName(bool fullPath, int index) {
    MessageTab* current = nullptr;
    if (index < 0) {
        current = CurrentMessage();
    } else if (index < static_cast<int>(messages.size())) {
        current = &messages[index];
    }
    if (current) {
        std::wstring text = fullPath ? current->path.wstring() : current->path.filename().wstring();
        if (!CopyTextToClipboard(window, text)) {
            ReportError("copyInformationFailed");
        }
    }
}

/// Reports whether the viewer has any recent files available to clear.
bool MainForm::HasRecentFiles() const {
    return !recentFiles.IsEmpty();
}

/// Applies the configured filename-length limit to every open tab caption.
void MainForm::UpdateTabNames() {
    for (int index = 0; index < static_cast<int>(messages.size()); ++index) {
        std::wstring title = MessageCaption(messages[index], settings.tabNameLength);
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = title.data();
        TabCtrl_SetItem(tabs, index, &item);
    }
}

/// Toggles the current view's persisted highlighting preference while preserving selection and scroll.
void MainForm::ToggleHighlighting() {
    MessageTab* current = CurrentMessage();
    if (!current || closing) {
        return;
    }
    AppSettings next = settings;
    bool& highlighted = current->sourceView ? next.highlightSource : next.highlightMessage;
    highlighted = !highlighted;
    if (!SaveSettings(next)) {
        ReportError("settingsFailed");
        return;
    }
    settings = next;
    CaptureViewState();
    HWND control = current->sourceView ? source : report.GetWindow();
    if (current->sourceView) {
        MessageView::DisplaySource(control, current->sourceText, settings.highlightSource);
    } else {
        report.Display(&current->document, language, settings.highlightMessage);
    }
    SendMessageW(control, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&current->selection));
    SendMessageW(control, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&current->scroll));
    UpdateCommands();
}

/// Opens formatted XML in a separate source tab or activates that message's existing source tab.
void MainForm::OpenSource() {
    MessageTab* current = CurrentMessage();
    if (!current || current->sourceView || closing) {
        return;
    }
    for (int index = 0; index < static_cast<int>(messages.size()); ++index) {
        if (messages[index].sourceView && messages[index].path == current->path) {
            SelectTab(index);
            return;
        }
    }
    MessageTab tab;
    tab.path = current->path;
    tab.document = current->document;
    tab.sourceView = true;
    std::vector<std::uint8_t> bytes = current->document.FormattedXml();
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), nullptr, 0);
    tab.sourceText.resize(length);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()), tab.sourceText.data(), length);
    int index = static_cast<int>(messages.size());
    std::wstring title = MessageCaption(tab, settings.tabNameLength);
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = title.data();
    messages.push_back(std::move(tab));
    if (TabCtrl_InsertItem(tabs, index, &item) < 0) {
        messages.pop_back();
        throw std::bad_alloc();
    }
    SelectTab(index);
}

/// Reveals the requested tab's original file or opens its containing folder when selection fails.
void MainForm::ShowInFolder(int index) {
    if (index < 0 || index >= static_cast<int>(messages.size())) {
        return;
    }
    const std::filesystem::path& path = messages[index].path;
    PIDLIST_ABSOLUTE item = nullptr;
    HRESULT result = SHParseDisplayName(path.c_str(), nullptr, &item, 0, nullptr);
    if (SUCCEEDED(result)) {
        result = SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
        CoTaskMemFree(item);
    }
    if (SUCCEEDED(result)) {
        return;
    }
    const std::filesystem::path folder = path.parent_path();
    SHELLEXECUTEINFOW operation{};
    operation.cbSize = sizeof(operation);
    operation.fMask = SEE_MASK_FLAG_NO_UI;
    operation.hwnd = window;
    operation.lpVerb = L"open";
    operation.lpFile = folder.c_str();
    operation.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&operation)) {
        ReportError("openFolderFailed");
    }
}

/// Appends a localized context-menu command without displaying its keyboard shortcut.
void MainForm::AppendContextMenuItem(HMENU menu, UINT id, const char* key, bool enabled) const {
    std::wstring caption = GetLocalizedText(language, key);
    size_t shortcut = caption.find(L'\t');
    if (shortcut != std::wstring::npos) {
        caption.resize(shortcut);
    }
    AppendMenuW(menu, MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED), id, caption.c_str());
}

/// Shows tab-header file commands or native read-only edit commands without shortcut labels.
void MainForm::ShowContextMenu(HWND control, LPARAM location) {
    if (control != tabs && control != status && control != report.GetWindow() && control != source) {
        return;
    }
    POINT point{ GET_X_LPARAM(location), GET_Y_LPARAM(location) };
    if (control == status) {
        ShowStatusContextMenu(point);
        return;
    }
    int index = activeIndex;
    if (control == tabs && location != -1) {
        TCHITTESTINFO hit{};
        hit.pt = point;
        ScreenToClient(tabs, &hit.pt);
        index = TabCtrl_HitTest(tabs, &hit);
        if (index < 0 || !(hit.flags & TCHT_ONITEM)) {
            return;
        }
    }
    if (location == -1) {
        RECT bounds{};
        if (control == tabs && index >= 0) {
            TabCtrl_GetItemRect(tabs, index, &bounds);
            MapWindowPoints(tabs, HWND_DESKTOP, reinterpret_cast<POINT*>(&bounds), 2);
        } else {
            GetWindowRect(control, &bounds);
        }
        point = POINT{ bounds.left, bounds.bottom };
    }
    HMENU menu = CreatePopupMenu();
    if (control == tabs) {
        if (index < 0) {
            DestroyMenu(menu);
            return;
        }
        AppendContextMenuItem(menu, ID_EDIT_COPY_FILE_NAME, "copyFileName");
        AppendContextMenuItem(menu, ID_EDIT_COPY_FULL_PATH, "copyFullPath");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendContextMenuItem(menu, ID_FILE_SHOW_IN_FOLDER, "showInFolder");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendContextMenuItem(menu, ID_FILE_CLOSE, "closeTabContext");
    } else {
        DWORD start = 0;
        DWORD end = 0;
        SendMessageW(control, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
        AppendContextMenuItem(menu, ID_EDIT_COPY, "copyContext", start != end);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendContextMenuItem(menu, ID_EDIT_SELECT_ALL, "selectAll", CanSelectAll(control, true));
    }
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0,
        window, nullptr);
    DestroyMenu(menu);
    if (control == tabs) {
        if (command == ID_FILE_CLOSE) {
            CloseTab(index);
        } else if (command == ID_FILE_SHOW_IN_FOLDER) {
            ShowInFolder(index);
        } else if (command == ID_EDIT_COPY_FILE_NAME || command == ID_EDIT_COPY_FULL_PATH) {
            CopyFileName(command == ID_EDIT_COPY_FULL_PATH, index);
        }
    } else if (command == ID_EDIT_SELECT_ALL) {
        SetFocus(control);
        SendMessageW(control, EM_SETSEL, 0, -1);
        SendMessageW(control, EM_HIDESELECTION, FALSE, 0);
    } else if (command == ID_EDIT_COPY) {
        SendMessageW(control, WM_COPY, 0, 0);
    }
}


/// Shows Copy for the clicked status pane and copies its complete Unicode text.
void MainForm::ShowStatusContextMenu(POINT point) {
    if (point.x == -1 && point.y == -1) {
        RECT bounds{};
        if (!SendMessageW(status, SB_GETRECT, 0, reinterpret_cast<LPARAM>(&bounds))) {
            return;
        }
        point = POINT{ bounds.left, (bounds.top + bounds.bottom) / 2 };
        ClientToScreen(status, &point);
    }
    POINT clientPoint = point;
    ScreenToClient(status, &clientPoint);
    RECT bounds{};
    if (!SendMessageW(status, SB_GETRECT, 0, reinterpret_cast<LPARAM>(&bounds))
            || !PtInRect(&bounds, clientPoint)) {
        return;
    }
    const wchar_t* caption = reinterpret_cast<const wchar_t*>(SendMessageW(status, SB_GETTEXTW, 0, 0));
    if (!caption) {
        return;
    }
    std::wstring text = caption;
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    AppendMenuW(menu, MF_STRING | (text.empty() ? MF_GRAYED : MF_ENABLED), ID_EDIT_COPY,
        GetLocalizedText(language, "copyContext"));
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0,
        window, nullptr);
    DestroyMenu(menu);
    if (command == ID_EDIT_COPY && !CopyTextToClipboard(window, text)) {
        ReportError("copyInformationFailed");
    }
}

/// Draws status text with native font metrics and fits paths to the pane without shortening copied data.
LRESULT MainForm::DrawStatus(const DRAWITEMSTRUCT* item) {
    HGDIOBJ old = SelectObject(item->hDC, font);
    TEXTMETRICW metrics{};
    GetTextMetricsW(item->hDC, &metrics);
    RECT text = item->rcItem;
    int borders[3]{};
    SendMessageW(status, SB_GETBORDERS, 0, reinterpret_cast<LPARAM>(borders));
    text.left += borders[2];
    text.right -= GetSystemMetrics(SM_CXVSCROLL);
    text.top += (text.bottom - text.top - metrics.tmHeight) / 2;
    SetTextColor(item->hDC, GetSysColor(COLOR_BTNTEXT));
    SetBkMode(item->hDC, TRANSPARENT);
    const wchar_t* caption = reinterpret_cast<const wchar_t*>(item->itemData);
    MessageTab* active = CurrentMessage();
    wchar_t pathCaption[MAX_PATH + 4];
    std::wstring displayCaption;
    UINT flags = DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;
    if (active && caption == statusText.c_str()) {
        const wchar_t* prefix = active->sourceView ? GetLocalizedText(language, "sourcePrefix") : L"";
        SIZE extent{};
        GetTextExtentPoint32W(item->hDC, prefix, lstrlenW(prefix), &extent);
        int width = text.right - text.left;
        FormatPathCaption(item->hDC, active->path.c_str(), width - extent.cx, pathCaption,
            ARRAYSIZE(pathCaption));
        displayCaption = prefix;
        displayCaption += pathCaption;
        caption = displayCaption.c_str();
        if (extent.cx <= width) {
            flags &= ~DT_END_ELLIPSIS;
        }
    }
    DrawTextW(item->hDC, caption, -1, &text, flags);
    SelectObject(item->hDC, old);
    return TRUE;
}
