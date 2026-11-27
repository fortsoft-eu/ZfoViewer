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

#include "ZfoViewer.h"

/// Manages compact native Preferences, staged values and session-only window placement.
class PreferencesForm {

public:
    /// Opens modal Preferences with staged settings and placement retained only for the running instance.
    void Show(HWND owner, HINSTANCE instance, AppSettings& settings, PersistWindowState& placement,
        AppLanguage language);

private:
    /// Dialog, owner and the system font currently used by its controls.
    HWND window = nullptr;
    HWND owner = nullptr;
    HFONT font = nullptr;

    /// Borrowed committed settings, applied language and session placement controller.
    AppSettings* settings = nullptr;
    AppLanguage language = LANG_EN_US;
    PersistWindowState* placement = nullptr;

    /// Staged message and source font choices, committed only by OK or Apply.
    ViewerFont messageFont;
    ViewerFont sourceFont;

    /// Ownership of a recreated system font.
    bool ownsFont = false;

    /// Scrollable content dimensions and wheel accumulation.
    int contentHeight = 0;
    int scrollPosition = 0;
    int wheelRemainder = 0;

    /// Suppresses change handling while controls are being populated or laid out.
    bool updating = false;

    /// Retains detected changes until the user successfully applies the preferences.
    bool applyPending = false;

    /// Applies the initial compact width before the first layout.
    void ResizeWidth() const;

    /// Limits the dialog to its monitor work area and exposes a native scrollbar only when content overflows.
    void FitHeight(int heightOfContent);

    /// Scrolls the dialog's existing child controls within the native vertical scrollbar range.
    bool Scroll(int request);

    /// Applies the system font and native control measurements, then lays out compact, baseline-aligned rows.
    void ApplyStyle(bool updateUi = true);

    /// Arranges existing controls using their current fonts without assigning a new interface font.
    void LayoutControls();

    /// Updates localized controls and layout together without painting intermediate positions or sizes.
    void RefreshLanguage();

    /// Describes both pending fonts and enables Default only for explicit custom selections.
    void UpdateFontButtons() const;

    /// Opens the native font dialog for the selected report or source font.
    void ChooseViewerFont(bool xmlSource);

    /// Compares pending controls and font choices with the last successfully applied preferences.
    bool PreferencesChanged() const;

    /// Retains detected changes and enables Apply until successful explicit application.
    void UpdateApply();

    /// Validates caption limits, saves staged preferences and synchronously notifies the owner with the
    /// previous settings.
    bool Apply();

    /// Handles native dialog initialization, settings commands, scrolling, mnemonics, theme and DPI changes.
    static INT_PTR CALLBACK DialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
};
