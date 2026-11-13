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
#include <richedit.h>
#include "Localization.h"
#include "ZfoDocument.h"
#include <vector>

/// Records the actual RichEdit font and displayed line height in pixels and document twips.
struct ReportFont {
    std::wstring family;
    LONG sizeTwips = 0;
    LONG lineTwips = 0;
    LONG linePixels = 0;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeout = false;
};

/// Measures the RichEdit's actual default character format, including its current DPI and zoom.
ReportFont GetReportFont(HWND edit);

/// Maps a RichEdit character range to an attachment action or a downloadable binary metadata field.
struct AttachmentLink {
    CHARRANGE range{};
    int attachment = -1;
    bool save = false;
    bool binary = false;
};

/// Renders decoded message metadata and clickable attachment links in one read-only RichEdit control.
class MessageView {

public:
    /// Creates the read-only RichEdit report and enables attachment links and selection notifications.
    bool Create(HWND owner, HINSTANCE instance);

    /// Returns the native RichEdit report control.
    HWND GetWindow() const;

    /// Builds localized message sections, formats their headings and records clickable attachment and binary
    /// links.
    void Display(const ZfoDocument* document, AppLanguage language, bool highlight = true);

    /// Loads formatted XML and colors its syntax without changing the selected font or source text.
    static void DisplaySource(HWND edit, const std::wstring& text, bool highlight = true);

    /// Formats the localized message or XML source as UTF-8-ready HTML 4.01 Strict or XHTML 1.1 markup.
    static std::wstring FormatHtml(HWND edit, const ZfoDocument& document, const std::wstring* source,
        const std::wstring& title, AppLanguage language, bool xhtml, bool highlight = true);

    /// Builds a standalone OpenDocument Text or Office Open XML package without an installed Office suite.
    static std::vector<std::uint8_t> FormatDocument(HWND edit, const ZfoDocument& document,
            const std::wstring* source, const std::wstring& title, AppLanguage language, bool docx,
            bool highlight = true);

    /// Returns the attachment or binary-data link covering the specified UTF-16 character position.
    const AttachmentLink* FindLink(LONG position) const;

    /// Returns the attachment under the selection start, or -1 for other text and binary-metadata links.
    int SelectedAttachment() const;

private:
    /// Native report control and link ranges rebuilt for the displayed message.
    HWND window = nullptr;
    std::vector<AttachmentLink> links;
};
