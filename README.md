# ZFO Viewer

[Česky](docs/README.cs-CZ.md) · [Slovensky](docs/README.sk-SK.md)

ZFO Viewer reads Czech ISDS messages and delivery receipts in a native Win32 interface.
Version 1.0.0.0 is under development and has not been released.

## Files and tabs

Use **File > Open** (`Ctrl+O`), **Open Folder**, drag and drop, or command-line filenames.
Each message opens in a separate tab. Opening the same file again selects its existing tab.
`Ctrl+Tab` and `Ctrl+Shift+Tab` switch tabs; `Ctrl+W` and a middle-click close a tab.
**File > Close All** closes every tab. The File menu remembers eight recent files.

**File > Save a Copy As** saves an unchanged copy of the original ZFO, including its envelope
and attachments. The current tab stays on the original file. **Exit** displays `Alt+F4`.
**File > Show in Folder** selects the active ZFO file in Explorer. The same command is in the tab menu.

The message view includes all XML metadata, including empty fields, attributes, and extension fields.
Dates and times follow the selected interface culture, including in exports. Times show hours and minutes
in Central European Time (CET/CEST), with localized zone abbreviations. Czech and Slovak dates use spaces
after the periods and no leading zeros. XML source and XML exports retain the original seconds, fractional
seconds and timezone offsets.
Click an attachment to open it, or use its adjacent Save link. Each tab extracts opened attachments into
its own short temporary directory, such as `%LOCALAPPDATA%\FortSoft\ZfoViewer\ZFO3A7C.tmp\invoice.pdf`.
Attachment filenames are preserved. Reopening an attachment in the same tab reuses an unchanged copy;
a different tab uses a different directory, even for identically named ZFO files.

Closing a tab attempts to remove its directory recursively. Locked files are skipped and retried
at startup, when opening attachments, when closing tabs, and when the application exits. Cleanup
preserves directories still owned by another running instance and does not follow directory links.

**View > View XML Source** (`Ctrl+U`) opens formatted XML in a new tab. Repeating the command for
that message selects its existing source tab. Tags, attributes, values, entities, and comments are
color-coded using the existing system font. High-contrast mode retains native text colors.
The XML view and XML export share the same formatting. The source command is disabled on an XML source tab.
**View > Color Highlighting** toggles the active view's highlighting and shares its saved preference with
Preferences. Message and XML highlighting remain independent; switching tabs updates the menu checkmark.

Text context menus contain Copy and Select All without shortcut labels. Tab context menus contain
Copy File Name, Copy Full Path, Show in Containing Folder, and Close Tab. The status bar can copy
its current text. The status bar displays Ready when empty and the active filename and path when open.

## Find, export, and print

**Edit > Find** (`Ctrl+F`), `F3`, and `Shift+F3` search messages, XML source, Help, and About.
Regular expressions use the statically linked C++ runtime's ECMAScript implementation with multiline
matching. PCRE2 is not used. Native RichEdit supplies text selection; the shared SelectionEdit
component handles the standard edit controls in Help and About.

**File > Export** (`Ctrl+E`) offers these formats in order:

1. ANSI text
2. UTF-8 text
3. UTF-8 text with BOM
4. UTF-16 big-endian text
5. UTF-16 little-endian text
6. Rich Text Format (RTF)
7. OpenDocument Text (ODT)
8. Office Open XML WordprocessingML Document (DOCX)
9. HyperText Markup Language 4.01 Strict (HTML)
10. Extensible HyperText Markup Language 1.1 (XHTML)
11. Formatted XML
12. BMP
13. GIF
14. JPEG
15. PNG
16. TIFF
17. Lossless WebP

Text encoders follow the established reference implementations. All six image formats use the same
ImageExport component as the saver applications: native Windows codecs for BMP, GIF, JPEG, PNG and TIFF,
and the bundled WebP library with PackedResource for lossless WebP.
ODT 1.2 and DOCX are generated directly from message metadata or XML source, using one built-in ZIP writer.
They preserve sections, bold labels, Unicode, spaces, tabs, and line breaks without requiring Office,
OpenOffice, LibreOffice, .NET, or an additional DLL.
HTML, XHTML, ODT, and DOCX take the font family, point size, and line height from the active RichEdit.
Image exports capture native RichEdit rendering, including off-screen content, current wrapping, emphasis,
and link colors. The original selection and scroll position are preserved. Images exceeding format or
memory limits are rejected without cropping the message.
HTML and XHTML use UTF-8 without BOM and preserve localized sections, metadata and attachment descriptions.
Exporting an XML source tab to either format preserves its text as escaped preformatted content.
RichEdit writes RTF directly. Native XmlLite formats XML. The supplied x86 libwebp DLL is compressed
into the executable and unpacked when needed. Icons and the DLL are in Resources.

**File > Page Setup** precedes **Print** (`Ctrl+P`). Printing also works in Help and About.
Windows 11 uses the modern print interface; the classic print path remains available on older systems.
Message and XML printing preserves the selected fonts, colors, bold text, italics and underlining.

## Preferences

**Options > Preferences** (`Ctrl+G`) lists Czech, Slovak, and US English, in that order.
The first launch follows the Windows UI language, with US English as the fallback. There is no
Automatic language option. A language selected by the user is remembered.

The first two rows choose the message and XML source fonts independently. Their Default buttons restore
the current system message font and Consolas 9 pt, respectively. These choices do not change the interface,
Help, or license fonts. Both color-highlighting options are enabled by default. Message highlighting only
colors section headings using the native link color; turning it off retains bold headings, labels, and attachment links.
XML highlighting follows Firefox's source styles, including bold tag and attribute names. HTML, XHTML, ODT,
DOCX, RTF, and image exports honor the selected content fonts and highlighting. ODT and DOCX are assembled
in memory and written directly to the destination without temporary files.

Preferences also contains the tab-name length (80 by default), the file-name length in the window title
(40 by default), closing tabs on double-click,
allowing multiple instances, smooth scrolling, and clearing recent files. OK and Apply save staged preferences;
Cancel discards changes since the last Apply. Clearing recent files takes effect immediately.
One application instance is the default; additional launches forward files to that window.
Smooth scrolling is off by default. Ordinary mouse-wheel scrolling in messages and XML source
moves immediately, using the system's line or page setting. Ctrl+wheel retains native RichEdit behavior.
Preferences uses compact native spacing and a vertical scrollbar when the work area is too short.

The interface uses the system font; the About license uses the reference monospaced font.
PersistWindowState restores window state and uses Windows
Default Bounds when no main-window bounds have been saved. About and Help follow the existing
application forms. Preferences and About retain their positions only in the running application instance.
Help and Find retain their window state in the registry. Search expressions and history remain session-only.
All three translations and Help text are in Localization.cpp.

Settings are stored under `HKEY_CURRENT_USER\Software\FortSoft\ZfoViewer`.
Boolean preferences share the Options DWORD: multiple instances uses `0x04`, and closing tabs
on double-click uses `0x40`, matching the reference bit positions. Smooth scrolling uses `0x80`.

## Build

Open ZfoViewer.slnx in Visual Studio 2026, or use a Developer Command Prompt:

```text
msbuild ZfoViewer.slnx /p:Configuration=Release /p:Platform=x86
```

The project targets Windows Vista and later, using v145, Windows SDK 10.0, Unicode Win32, C++20,
and a statically linked C++ runtime in Release. Compiler warnings use level 4 (`/W4`) in both
configurations. SDK and linker defaults follow the reference projects, without explicit Windows-version
overrides. Windows XP is unsupported.
Build outputs go to the solution root, under the selected configuration name, as in the reference projects.

The resource preparation step packs the supplied WebP DLL. PackageRelease.ps1 is based on the
existing signing and packaging workflow and operates on an already-built Release executable.
Packaging and signing are separate user actions; a normal build does not create a ZIP.

Temporary tests remain under tests and are excluded from Git. Their executables, objects, fixtures,
and renderings stay under that directory. Run tests/RunTests.ps1 to execute them. GUI tests use an
isolated desktop, nonactivating windows, isolated registry data, and a private clipboard substitute.
They do not activate the desktop, show print dialogs, or submit physical print jobs.

## Signature support and license

Crypt32 decodes CMS/PKCS#7, and MSXML 6 parses the ISDS XML. External XML resources and DTDs are disabled.
External attachment references and legacy Software602 forms are unsupported. Envelope signature
verification checks the cryptographic signature only; certificate trust, revocation, timestamps,
and long-term validity are not verified.

The application is MIT-licensed. The bundled libwebp uses its own license; see [license.txt](license.txt).
