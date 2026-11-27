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
#include "Localization.h"
#include "PersistWindowState.h"
#include <vector>
#include <filesystem>

/// Retains a selected LOGFONT and its original DPI; an empty face name selects the viewer default.
struct ViewerFont {
    LOGFONTW font{};
    UINT dpi = 96;
};

/// Stores committed viewer preferences and the distinct About and Help placement policies.
struct AppSettings {
    /// Boolean preferences packed into the shared registry Options value.
    bool multipleInstances = false;
    bool closeTabsOnDoubleClick = false;
    bool smoothScrolling = false;
    bool highlightMessage = true;
    bool highlightSource = true;
    bool lowercaseExtensions = true;

    /// Independent report and XML source font selections.
    ViewerFont messageFont;
    ViewerFont sourceFont;

    /// Independent filename-length limits for tab captions and the main window title.
    UINT tabNameLength = 80;
    UINT titleNameLength = 40;

    /// Selected interface language; -1 follows Windows until the user makes an explicit selection.
    int language = -1;

    /// Session-only About placement and registry-backed Help placement.
    PersistWindowStateData aboutWindowState;
    PersistWindowStateData helpWindowState;
};

/// Main-window identity and messages used to forward paths between application instances.
static constexpr wchar_t MainWindowClass[] = L"FortSoft.ZfoViewer.MainForm";
static constexpr ULONG_PTR CopyDataIdentifier = 0x5A464F31;
static constexpr UINT OpenForwardedFilesMessage = WM_APP + 20;

/// Distinguishes a missing receiving window from accepted and failed single-instance forwarding.
enum class InstanceForwardResult {
    NoWindow,
    Forwarded,
    Failed
};

/// Serializes application startup and forwards file-open requests to an existing viewer window.
class SingleInstance {

public:
    /// Acquires the named startup mutex to serialize detection and creation of the first application window.
    explicit SingleInstance(const std::wstring& name);

    /// Releases the startup lock if held and closes the mutex handle.
    ~SingleInstance();

    /// Forwards a double-null-terminated path list to an existing viewer using bounded synchronous IPC.
    InstanceForwardResult ForwardToExisting(const std::vector<std::filesystem::path>& files) const;

    /// Reports whether this instance currently owns the startup mutex.
    bool IsLocked() const;

    /// Releases the startup mutex after forwarding or creating the receiving main window.
    void ReleaseStartup();

private:
    /// Named startup mutex and whether this instance currently owns it.
    HANDLE mutex = nullptr;
    bool locked = false;
};

struct PrintJob;

class PrintSettings;

/// Notifications exchanged by the shared Find and Help forms and their owner.
static const UINT findVisibilityChangedMessage = WM_APP + 6;
static const UINT findMatchMessage = WM_APP + 7;
static const UINT helpVisibilityChangedMessage = WM_APP + 8;

/// Product identity and website used by the shared information forms.
inline constexpr wchar_t PRODUCT_NAME[] = L"ZFO Viewer";
inline constexpr wchar_t WEBSITE_URL[] = L"https://fortsoft.eu/zfoviewer/";

/// Subclass identifier for information-edit dialog navigation.
inline constexpr UINT_PTR INFORMATION_TEXT_SUBCLASS_ID = 0xCC01;

/// Loads saved language, caption limits, packed option bits and Help placement from the registry.
void LoadSettings(AppSettings& settings);

/// Persists Help placement only; About placement stays in memory for the running instance.
void SaveWindowPositions(const AppSettings& settings);

/// Commits language, caption limits and controlled option bits under a named mutex, preserving other bits.
bool SaveSettings(const AppSettings& settings);

/// Reads a validated, double-null-terminated recent-file list and returns its length in UTF-16 units.
DWORD LoadRecentFilesSetting(wchar_t* paths, DWORD capacity);

/// Stores the double-null-terminated recent-file list as a registry multi-string value.
bool SaveRecentFilesSetting(const wchar_t* paths, DWORD length);

/// Returns the saved export-format identifier, defaulting to UTF-8 text when it is absent or invalid.
DWORD LoadExportType();

/// Remembers the selected export format for the next export dialog.
void SaveExportType(DWORD type);

/// Reports whether visual styles are active for Windows and the current application.
bool AreVisualStylesActive();

/// Returns the effective DPI through the shared system-dialog implementation.
UINT WindowDpi(HWND window);

/// Creates a selected font at the target DPI, using the system font or Consolas for an empty selection.
HFONT CreateViewerFont(HWND window, const ViewerFont& saved, bool xmlSource);

/// Compares complete native font settings, including the DPI at which they were chosen.
bool EqualViewerFonts(const ViewerFont& left, const ViewerFont& right);

/// Creates an owned system dialog font for the target window's DPI.
HFONT CreateUiFont(HWND window);

/// Reloads small and large caption icons from the requested resource at the window's current DPI.
void UpdateWindowIcons(HWND window, UINT resource);

/// Removes and destroys the small and large icons owned through window properties.
void ClearWindowIcons(HWND window);

/// Converts dialog units to pixels using the supplied font's native base-unit measurements.
SIZE DialogUnits(HFONT font, int x, int y);

/// Measures the button's ideal content size and retains the supplied native minimum dimensions.
SIZE PreferredButtonSize(HWND button, const SIZE& minimum);

/// Returns a control's window bounds mapped into its parent's client coordinates.
RECT ControlRect(HWND window, HWND parent);

/// Positions and sizes a control without activation, Z-order changes or copied stale pixels.
void PlaceControl(HWND window, int x, int y, int width, int height);

/// Returns the native selected-text rectangle used for combo-box baseline alignment.
bool ComboBoxTextRect(HWND combo, RECT* rectangle);

/// Fits native combo selection and list-item heights to its current font and system borders.
void UpdateComboBoxMetrics(HWND combo);

/// Measures the dropdown from its current font and items and keeps it within the monitor work area.
void UpdateComboBoxDropDownSize(HWND combo);

/// Calculates the rendered text baseline in screen coordinates from native control content and font metrics.
LONG ControlTextBaseline(HWND control);

/// Moves a control vertically to match the reference control's rendered text baseline.
void AlignControlBaseline(HWND control, HWND reference);

/// Fits an edit's text inset and height to current font and theme metrics, including edits inside combo
/// boxes.
void UpdateEditMetrics(HWND edit, HWND enclosingCombo = nullptr);

/// Reads text metrics from the control's current font and reports whether measurement succeeded.
bool ReadControlMetrics(HWND control, TEXTMETRICW* metrics);

/// Copies Unicode text to the clipboard, transferring buffer ownership only after a successful copy.
bool CopyTextToClipboard(HWND owner, const wchar_t* text);

/// Copies edit text into a caller-owned process-heap buffer, preserving RichEdit UTF-16 selection offsets.
wchar_t* CopyControlText(HWND control);

/// Paints owner-drawn product text using the control font, enabled-state color and parent background.
INT_PTR DrawWrappedTextItem(const DRAWITEMSTRUCT* item);

/// Uses Windows Unicode character classification to identify whitespace.
bool IsTextWhitespace(wchar_t character);

/// Releases cached Uniscribe analysis and its associated UTF-16 text.
void ResetInformationWordBreak();

/// Captures text, font, DPI, margins and title into a print snapshot independent of later edits.
PrintJob* CapturePrintJob(HWND owner, HWND edit, const wchar_t* title, const RECT& margins);

/// Captures the active report and attaches the printer selected through the classic print dialog.
PrintJob* ChooseReportPrinter(HWND owner, HWND edit, const wchar_t* title, PrintSettings& settings,
    wchar_t* error, int errorCapacity);

/// Paginates and prints the captured text with its font and margins while honoring cancellation.
bool PrintReportPages(PrintJob* job);

/// Uses the application's product caption for diagnostics raised by shared components.
int WINAPI ComponentMessageBoxW(HWND owner, LPCWSTR text, LPCWSTR title, UINT type);
#ifndef MessageBoxW
#define MessageBoxW ComponentMessageBoxW
#endif

/// Returns an owned copy of a control's current Unicode text.
std::wstring GetControlText(HWND control);

/// Returns the requested localized control caption with its existing mnemonic markers.
std::wstring Mnemonic(AppLanguage language, TextId id);

/// Copies Unicode text to the clipboard, transferring buffer ownership only after a successful copy.
bool CopyTextToClipboard(HWND owner, const std::wstring& text);

/// Exposes Windows Unicode whitespace classification to shared information forms.
bool IsUnicodeWhitespace(wchar_t character);

/// Leaves screen-saver inhibition disabled for this document viewer.
bool ShouldInhibitScreenSaver();

/// Converts current information-dialog font units to native pixel dimensions.
SIZE InformationDialogUnits(HFONT font, int x, int y);

/// Measures the common native Close button using its current font and existing dialog-unit minimum and
/// padding.
SIZE InformationButtonSize(HWND window, HFONT font, int resourceId);

/// Measures a label's wrapped height at the requested width using its actual font, or returns zero if a DC is
/// unavailable.
int InformationLabelHeight(HWND control, int width);

/// Positions an information-window child without activation or Z-order changes, enforcing positive
/// dimensions.
void PlaceInformationControl(HWND window, int id, int x, int y, int width, int height);

/// Aligns a settings control with the reference text baseline, including an optional measured baseline
/// distance.
void AlignSettingsControl(HWND control, HWND reference, LONG baselineDistance = 0);

/// Creates a nonactivating native tooltip for a child control and returns its owned window handle.
HWND CreateControlTooltip(HWND parent, HWND control, const wchar_t* text);

/// Obtains a native Windows default position with a hidden reference window and applies it without
/// activation.
bool PositionInformationWindowByWindowsDefault(HWND window);

/// Requests localized Help and About refresh through the owning main form.
void RefreshInformationWindows(HWND owner);

/// Retains the shared information-form hook without changing this viewer's window Z order.
void UpdateInformationWindowsTopmost(HWND owner);

/// Provides native edit word movement from cached Unicode analysis, with a character-class fallback on
/// failure.
int CALLBACK InformationWordBreakProc(wchar_t* text, int position, int length, int code);

/// Keeps Return, Escape and Tab available to native dialog navigation and removes the subclass on
/// destruction.
LRESULT CALLBACK InformationTextSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR referenceData);

/// Shortens a long filename in the middle while retaining its beginning, stem ending and complete extension.
void FormatFileNameCaption(const wchar_t* path, UINT limit, wchar_t* caption, DWORD capacity);

/// Fits an absolute path to its current font and width, retaining the root, parent directory and both
/// filename ends.
void FormatPathCaption(HDC dc, const wchar_t* path, int width, wchar_t* caption, DWORD capacity);

/// Synchronous preference-commit notification carrying a borrowed previous-settings snapshot.
inline constexpr UINT PreferencesAppliedMessage = WM_APP + 24;

/// Owns caption icons cached by DPI for the shared information forms.
class ProgramIcons {
    /// Retains the small and large application icons at one effective window DPI.
    struct IconPair {
        UINT dpi;
        HICON large;
        HICON smallIcon;
    };

public:
    /// Destroys every owned caption-icon pair cached for the application's information windows.
    ~ProgramIcons();

    /// Checks that the embedded application icon is available before creating information forms.
    bool Load();

    /// Loads an owned application icon at the requested pixel dimensions.
    HICON LoadSized(UINT size) const;

    /// Applies small and large caption icons, reusing the cached pair for the window's DPI.
    void Apply(HWND window) const;

private:
    /// Icon pairs owned until the application icon provider is destroyed.
    mutable std::vector<IconPair> icons;
};

/// Returns the nearest monitor's work area, using the system work area if monitor information is unavailable.
RECT WindowWorkArea(const RECT& rectangle);

/// Describes an export identifier, localized filter label, filename pattern and default extension.
struct ExportChoice {
    DWORD type;
    TextId label;
    const wchar_t* pattern;
    const wchar_t* extension;
};

/// Export filters in menu order: five text encodings, RTF, ODT, DOCX, HTML, XHTML, XML and six image formats.
extern const ExportChoice exportChoices[17];

/// Captures native RichEdit rendering at its current layout and saves it with the shared image encoder.
bool SaveReportImage(HWND edit, const wchar_t* path, DWORD type, wchar_t* error, int errorCapacity);

/// Saves the report in the selected text encoding, replacing the destination only after a complete write.
bool SaveReportText(const wchar_t* text, const wchar_t* path, DWORD type, wchar_t* error, int errorCapacity);

/// Streams RichEdit formatting to a temporary file and replaces the target only after a successful export.
bool SaveReportRtf(HWND edit, const wchar_t* path, wchar_t* error, int capacity);

/// Resolves a filesystem path to an absolute Windows path and reports path-resolution failures.
std::filesystem::path AbsolutePath(const std::filesystem::path& path);

std::wstring InformationHelpText(AppLanguage language, bool applicationMode);
void SaveInformationWindowPositions(const AppSettings& settings);
HICON LoadInformationFormIcon(HINSTANCE instance, int size);
void ApplyInformationFormIcons(HWND window, HINSTANCE instance, int iconIndex = 0);

bool TranslateModalInformationMessage(HWND owner, MSG& message);
INT_PTR ShowModalInformationForm(HINSTANCE instance, const DLGTEMPLATE* definition, HWND owner,
    DLGPROC procedure, LPARAM parameter);
