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

#pragma once

#include <windows.h>
#include <shobjidl.h>
#include "ZfoViewer.h"
#include "AboutForm.h"
#include "HelpForm.h"
#include "FindForm.h"
#include "PersistWindowState.h"
#include "PrintSettings.h"
#include "Localization.h"
#include "MessageView.h"
#include "RecentFiles.h"
#include "TemporaryDirectory.h"
#include <memory>
#include <filesystem>
#include <vector>

/// Owns one message or XML-source tab, its view state and its independent attachment cache.
struct MessageTab {
    std::filesystem::path path;
    ZfoDocument document;
    std::unique_ptr<TemporaryDirectory> temporary;
    std::wstring sourceText;
    bool sourceView = false;
    POINT scroll{};
    CHARRANGE selection{};
};

/// Retains one native RichEdit character-format run for the modern vector-printing path.
struct PrintTextRun {
    UINT start;
    UINT length;
    COLORREF color;
    LONG heightTwips;
    LONG weight;
    bool italic;
    bool underline;
    bool strikeOut;
    wchar_t family[LF_FACESIZE];
};

/// Owns a printer context and a report snapshot for one cancellable background print job.
struct PrintJob {
    HWND target;
    HDC printer;
    wchar_t* report;
    BYTE* richText;
    ULONG richTextSize;
    PrintTextRun* formats;
    UINT formatCount;
    HRESULT(*formatLayout)(const PrintJob* job, IUnknown* layout, IUnknown* context, UINT start, UINT length);
    LOGFONTW font;
    RECT margins;
    UINT fontDpi;
    wchar_t title[MAX_PATH];
    volatile LONG canceled;
    bool printToFile;
    bool success;
};

/// Owns the native viewer, message tabs, information forms and print sessions.
class MainForm {

public:
    /// Initializes the viewer's interface language and loads its recent-file history.
    explicit MainForm(AppSettings& settings);

    /// Stops printing and releases information forms, message data, temporary files, fonts and RichEdit.
    ~MainForm();

    /// Creates the main window and connects its placement to persisted state and Windows default bounds.
    bool Create(HINSTANCE instance);

    /// Opens a ZFO message or selects its existing message tab and updates recent-file history.
    bool OpenFile(const std::filesystem::path& path);

    /// Routes shortcuts and modeless dialog navigation only to the applicable main, search or information
    /// window.
    bool Translate(MSG& message);

    /// Returns the native main window, or null before it is created.
    HWND GetWindow() const;

    /// Displays the main window using its saved placement or Windows default bounds.
    void Show();

    /// Refreshes the open Help and About forms in the selected interface language.
    void RefreshInformation();

    /// Reports whether the viewer has any recent files available to clear.
    bool HasRecentFiles() const;

private:
    /// Main window, native child controls and windows used for search and print routing.
    HWND window = nullptr;
    HWND tabs = nullptr;
    HWND status = nullptr;
    HWND source = nullptr;
    HWND lastFocusedControl = nullptr;
    HWND searchWindow = nullptr;
    HWND printOwner = nullptr;

    /// Native accelerators, module handles, system font and background print-thread handle.
    HACCEL accelerators = nullptr;
    HFONT font = nullptr;
    HFONT messageFont = nullptr;
    HFONT sourceFont = nullptr;
    HINSTANCE instance = nullptr;
    HMODULE richEdit = nullptr;
    HANDLE printThread = nullptr;

    /// Committed preferences and the currently applied interface language.
    AppSettings& settings;
    AppLanguage language = LANG_EN_US;

    /// Active tab index and high-resolution wheel accumulation for the two RichEdit views.
    int activeIndex = -1;
    int wheelRemainder[2]{};

    /// Complete status text retained for native owner drawing and copying.
    std::wstring statusText;

    /// Owned message tabs and file-open requests queued for the main UI thread.
    std::vector<MessageTab> messages;
    std::vector<std::filesystem::path> pendingFiles;

    /// Shutdown and print-dialog state, plus the currently active classic or modern print session.
    bool closing = false;
    bool printDialogOpen = false;
    PrintJob* activePrintJob = nullptr;
    class ModernPrintDialog* modernPrint = nullptr;

    /// Placement controllers for the main window, session-only dialogs and registry-backed Help.
    PersistWindowState placement;
    PersistWindowState aboutPlacement;
    PersistWindowState preferencesPlacement;
    PersistWindowState helpPlacement;

    /// Native icons, recent-file history, print preferences, report rendering and modeless information forms.
    ProgramIcons icons;
    RecentFiles recentFiles;
    PrintSettings printSettings;
    MessageView report;
    AboutForm about;
    HelpForm help;
    FindForm find;

    /// Returns the active message or XML-source tab, or null when no tab is selected.
    MessageTab* CurrentMessage();

    /// Returns the active information edit or the current message or XML edit for copy and print commands.
    HWND TextTarget();

    /// Restores focus to the previous available child control or the active message or XML edit.
    void RestoreFocus();

    /// Selects the edit in the last active searchable form while retaining it during search-form input.
    void UpdateFindTarget();

    /// Shows the current RichEdit search selection while Find is open and restores native hiding otherwise.
    void UpdateSearchSelection();

    /// Captures the active text view and starts the modern print dialog or the classic background print path.
    void PrintReport();

    /// Joins a completed classic print job, releases its snapshot and reports failures unless closing.
    void CompletePrint(PrintJob* job);

    /// Releases a completed modern print session and refreshes print-command availability.
    void CompleteModernPrint(class ModernPrintDialog* source);

    /// Opens native Page Setup for the active text view and reports configuration failures.
    void PageSetup();

    /// Prints a captured report on the worker thread and posts completion back to the main window.
    static DWORD WINAPI PrintDocument(void* parameter);

    /// Creates the native tabs, status bar and RichEdit controls and installs their message handlers.
    void CreateControls();

    /// Refreshes menus, captions and information forms while preserving the active document's selection and
    /// scroll.
    void RefreshLanguage();

    /// Uses native owner-relative bounds on the first About opening, then retains its position within this
    /// instance.
    void ShowAbout();

    /// Creates menus once, then updates their localized captions, recent files and command availability in
    /// place.
    void BuildMenu();

    /// Applies the system dialog font and selects native RichEdit borders according to actual visual-style
    /// state.
    void SetFonts();

    /// Applies independent content fonts without changing the application or information-dialog fonts.
    void SetContentFonts();

    /// Fits the status bar and tab workspace to the client area and invalidates resized RichEdit content.
    void Layout();

    /// Shortens a complete filename caption, including the localized XML-source prefix when applicable.
    std::wstring MessageCaption(const MessageTab& message, UINT limit) const;

    /// Displays the active message or formatted XML and restores that tab's selection and scroll position.
    void Populate();

    /// Refreshes edit, search, print and file-command availability and the current status-bar text.
    void UpdateCommands();

    /// Retains the active tab's RichEdit selection and scroll position before its view changes.
    void CaptureViewState();

    /// Saves the previous view, resets search state when needed and displays the requested tab.
    void SelectTab(int index);

    /// Removes one tab and its extracted files, then selects the appropriate remaining tab.
    void CloseTab(int index);

    /// Shows the native picker for messages or a folder and opens the selected filesystem items.
    void OpenDialog(bool folder = false);

    /// Opens ZFO files directly inside the selected folder in sorted order, reusing existing message tabs.
    void OpenFolder(const std::filesystem::path& path);

    /// Closes every tab, attempts temporary-file cleanup and returns to the empty workspace.
    void CloseAll();

    /// Applies the configured filename-length limit to every open tab caption.
    void UpdateTabNames();

    /// Updates the window title from the active tab using its independent name-length limit.
    void UpdateWindowTitle();

    /// Copies the complete filename or full path for the requested tab, independently of caption shortening.
    void CopyFileName(bool fullPath, int index = -1);

    /// Opens the requested tab's containing folder and selects its original ZFO file.
    void ShowInFolder(int index);

    /// Appends a localized context-menu command without displaying its keyboard shortcut.
    void AppendContextMenuItem(HMENU menu, UINT id, const char* key, bool enabled = true) const;

    /// Shows tab-header file commands or native read-only edit commands without shortcut labels.
    void ShowContextMenu(HWND control, LPARAM location);

    /// Shows Copy for the clicked status pane and copies its complete Unicode text.
    void ShowStatusContextMenu(POINT point);

    /// Draws status text with native font metrics and fits paths to the pane without shortening copied data.
    LRESULT DrawStatus(const DRAWITEMSTRUCT* item);

    /// Saves an unchanged copy of the active tab's original ZFO file to the user-selected path.
    void SaveCopyAs();

    /// Saves the requested or selected message attachment using its original decoded bytes.
    void SaveAttachment(int index = -1);

    /// Saves the decoded binary payload associated with a metadata link.
    void SaveBinaryField(int index);

    /// Reuses or extracts an attachment in its tab's private temporary directory and opens it in its
    /// associated application.
    void OpenAttachment(int index = -1);

    /// Exports the active report as text, Office documents, RTF, web markup, formatted XML or an image.
    void ExportReport();

    /// Toggles the current view's persisted highlighting preference while preserving selection and scroll.
    void ToggleHighlighting();

    /// Opens formatted XML in a separate source tab or activates that message's existing source tab.
    void OpenSource();

    /// Copies the selected text from the active message, XML, Help or About edit through its native control.
    void CopySelection();

    /// Displays a localized error, using the generic diagnostic when the translation key is unknown.
    void ReportError(const char* key) const;

    /// Applies the filename-extension preference without changing the filename stem or parent directory.
    void ApplyExtensionCase(std::filesystem::path& path) const;

    /// Shows the native Save As dialog and returns its selected path and optional export-format index.
    std::filesystem::path SaveDialog(const std::wstring& name, const wchar_t* extension,
        const COMDLG_FILTERSPEC* filters = nullptr, UINT count = 0, UINT* type = nullptr,
        const wchar_t* title = nullptr) const;

    /// Opens Preferences and releases the dialog object when the modal session ends.
    void ShowPreferences();

    /// Applies the suggested DPI bounds before rebuilding the main window fonts and layout.
    void ApplyDpiBounds(const RECT& bounds);

    /// Dispatches menu commands while preserving command-specific lifetime boundaries.
    LRESULT HandleCommand(UINT command);

    /// Handles status, tab, tooltip and RichEdit notifications with their native return values.
    LRESULT HandleNotification(LPARAM lParam);

    /// Collects dropped paths and releases the drop handle before opening the requested messages.
    void OpenDroppedFiles(WPARAM wParam);

    /// Validates and queues file paths received from another application instance.
    LRESULT QueueForwardedFiles(LPARAM lParam);

    /// Opens queued paths before restoring and activating the existing application window.
    void OpenForwardedFiles();

    /// Applies changed fonts, colors, language and caption limits while preserving the active view.
    void ApplyPreferences(const AppSettings& previous);

    /// Paints the empty workspace while keeping BeginPaint and EndPaint in the same handler.
    void Paint();

    /// Handles main-window commands, tab and RichEdit notifications, printing, placement and lifecycle
    /// messages.
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    /// Handles dropped files and optional immediate wheel scrolling, then removes its subclass on
    /// destruction.
    static LRESULT CALLBACK RichEditProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR referenceData);

    /// Associates the native window with its form object and converts uncaught failures into localized
    /// errors.
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
};
