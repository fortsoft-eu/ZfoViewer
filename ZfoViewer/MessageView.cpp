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

#include "MessageView.h"
#include "resource.h"
#include "SystemDialogTemplate.h"
#include <algorithm>
#include <cwchar>
#include <commctrl.h>
#include <array>
#include <limits>
#include <stdexcept>
#include <string_view>

/// Associates a declared MIME type with its localized file-format name.
struct MimeDescription {
    std::wstring_view type;
    TextId name;
};

/// Identifies the Firefox source-view character styles shared by native display and rich exports.
enum class SourceToken : size_t {
    Tag,
    Attribute,
    Value,
    Comment,
    Entity,
    Declaration,
    ProcessingInstruction,
    CData
};

/// Defines one source token's foreground, emphasis and exported character-style name.
struct SourceStyle {
    COLORREF color;
    DWORD effects;
    const wchar_t* name;
};

/// Associates one nonoverlapping UTF-16 range with a source character style.
struct SourceSpan {
    CHARRANGE range{};
    SourceToken token = SourceToken::Tag;
};

/// Retains a parsed XML date or timestamp and its explicit UTC offset for display conversion.
struct FieldDateTime {
    SYSTEMTIME value{};
    LONG offsetMinutes = 0;
    bool hasTime = false;
    bool hasZone = false;
};

/// Holds the native registry layout of one Windows time zone's conversion rules.
struct RegistryTimeZone {
    LONG bias;
    LONG standardBias;
    LONG daylightBias;
    SYSTEMTIME standardDate;
    SYSTEMTIME daylightDate;
};

/// Defines the readable date and minute-only time patterns for an interface culture.
struct MessageDateFormat {
    const wchar_t* locale;
    const wchar_t* date;
    const wchar_t* time;
};

/// Holds one localized report paragraph shared by document and web exports.
struct ReportParagraph {
    std::wstring label;
    std::wstring text;
    bool heading = false;
    bool attachment = false;
    std::vector<SourceSpan> sourceStyles;
};

/// Owns one ASCII-named ZIP part and the metadata needed by its central-directory entry.
struct DocumentPart {
    std::string name;
    std::vector<std::uint8_t> data;
    std::uint32_t crc = 0;
    std::uint32_t offset = 0;
};

#pragma pack(push, 1)

/// Defines the little-endian local header for a stored ZIP entry on Windows.
struct ZipEntryHeader {
    std::uint32_t signature = 0x04034B50;
    std::uint16_t version = 20;
    std::uint16_t flags = 0;
    std::uint16_t method = 0;
    std::uint16_t time = 0;
    std::uint16_t date = 0x0021;
    std::uint32_t crc = 0;
    std::uint32_t compressedSize = 0;
    std::uint32_t size = 0;
    std::uint16_t nameLength = 0;
    std::uint16_t extraLength = 0;
};

/// Defines the central-directory record corresponding to one stored ZIP entry.
struct ZipDirectoryHeader {
    std::uint32_t signature = 0x02014B50;
    std::uint16_t creatorVersion = 20;
    std::uint16_t version = 20;
    std::uint16_t flags = 0;
    std::uint16_t method = 0;
    std::uint16_t time = 0;
    std::uint16_t date = 0x0021;
    std::uint32_t crc = 0;
    std::uint32_t compressedSize = 0;
    std::uint32_t size = 0;
    std::uint16_t nameLength = 0;
    std::uint16_t extraLength = 0;
    std::uint16_t commentLength = 0;
    std::uint16_t disk = 0;
    std::uint16_t internalAttributes = 0;
    std::uint32_t externalAttributes = 0;
    std::uint32_t offset = 0;
};

/// Completes the single-disk ZIP package without a comment or a ZIP64 extension.
struct ZipDirectoryEnd {
    std::uint32_t signature = 0x06054B50;
    std::uint16_t disk = 0;
    std::uint16_t directoryDisk = 0;
    std::uint16_t diskEntries = 0;
    std::uint16_t entries = 0;
    std::uint32_t size = 0;
    std::uint32_t offset = 0;
    std::uint16_t commentLength = 0;
};

#pragma pack(pop)

static_assert(sizeof(ZipEntryHeader) == 30);
static_assert(sizeof(ZipDirectoryHeader) == 46);
static_assert(sizeof(ZipDirectoryEnd) == 22);

static constexpr wchar_t DocumentXmlHeader[] = L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";

static constexpr MessageDateFormat MessageDateFormats[] = {
    { L"cs-CZ", L"d. M. yyyy", L"H:mm" },
    { L"sk-SK", L"d. M. yyyy", L"H:mm" },
    { L"en-US", L"M/d/yyyy", L"h:mm tt" }
};

static_assert(std::size(MessageDateFormats) == LANG_COUNT);

static constexpr wchar_t CentralEuropeTimeZoneKey[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\Central Europe Standard Time";

static constexpr const char* DateFields[] = { "dmDeliveryTime", "dmAcceptanceTime", "dmEventTime", "biDate" };

static constexpr SourceStyle SourceStyles[] = {
    { RGB(128, 0, 128), CFE_BOLD, L"XmlTag" },
    { CLR_DEFAULT, CFE_BOLD, L"XmlAttribute" },
    { RGB(0, 0, 255), 0, L"XmlValue" },
    { RGB(0, 128, 0), CFE_ITALIC, L"XmlComment" },
    { RGB(255, 69, 0), 0, L"XmlEntity" },
    { RGB(70, 130, 180), CFE_ITALIC, L"XmlDeclaration" },
    { RGB(218, 112, 214), CFE_ITALIC, L"XmlProcessingInstruction" },
    { RGB(204, 0, 102), 0, L"XmlCData" }
};

/// Localization keys for message sections in their displayed order.
static constexpr const char* SectionKeys[] = {
    "generalSection",
    "senderSection",
    "senderPersonSection",
    "recipientSection",
    "additionalSection",
    "statusSection",
    "eventsSection",
    "signatureSection"
};

static constexpr MimeDescription MimeDescriptions[] = {
    { L"application/octet-stream", TXT_MIME_BINARY },
    { L"application/encrypted", TXT_MIME_ENCRYPTED },
    { L"application/pdf", TXT_MIME_PDF },
    { L"text/plain", TXT_MIME_TEXT },
    { L"text/csv", TXT_MIME_CSV },
    { L"application/rtf", TXT_MIME_RTF },
    { L"text/rtf", TXT_MIME_RTF },
    { L"application/x-rtf", TXT_MIME_RTF },
    { L"text/html", TXT_MIME_HTML },
    { L"application/xhtml+xml", TXT_MIME_XHTML },
    { L"application/xml", TXT_MIME_XML },
    { L"text/xml", TXT_MIME_XML },
    { L"application/json", TXT_MIME_JSON },
    { L"application/msword", TXT_MIME_DOC },
    { L"application/vnd.openxmlformats-officedocument.wordprocessingml.document", TXT_MIME_DOCX },
    { L"application/vnd.ms-excel", TXT_MIME_XLS },
    { L"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", TXT_MIME_XLSX },
    { L"application/vnd.ms-powerpoint", TXT_MIME_PPT },
    { L"application/vnd.openxmlformats-officedocument.presentationml.presentation", TXT_MIME_PPTX },
    { L"application/vnd.oasis.opendocument.text", TXT_MIME_ODT },
    { L"application/vnd.oasis.opendocument.spreadsheet", TXT_MIME_ODS },
    { L"application/vnd.oasis.opendocument.presentation", TXT_MIME_ODP },
    { L"image/png", TXT_MIME_PNG },
    { L"image/x-png", TXT_MIME_PNG },
    { L"image/gif", TXT_MIME_GIF },
    { L"image/jpeg", TXT_MIME_JPEG },
    { L"image/pjpeg", TXT_MIME_JPEG },
    { L"image/tiff", TXT_MIME_TIFF },
    { L"image/heic", TXT_MIME_HEIC },
    { L"image/heic-sequence", TXT_MIME_HEIC_SEQUENCE },
    { L"image/heif", TXT_MIME_HEIF },
    { L"image/heif-sequence", TXT_MIME_HEIF_SEQUENCE },
    { L"image/bmp", TXT_MIME_BMP },
    { L"image/x-bmp", TXT_MIME_BMP },
    { L"image/x-ms-bmp", TXT_MIME_BMP },
    { L"image/webp", TXT_MIME_WEBP },
    { L"image/svg+xml", TXT_MIME_SVG },
    { L"audio/wav", TXT_MIME_WAV },
    { L"audio/wave", TXT_MIME_WAV },
    { L"audio/x-wav", TXT_MIME_WAV },
    { L"audio/vnd.wave", TXT_MIME_WAV },
    { L"audio/mpeg", TXT_MIME_MPEG_AUDIO },
    { L"audio/mp3", TXT_MIME_MPEG_AUDIO },
    { L"audio/mp2", TXT_MIME_MPEG_AUDIO },
    { L"audio/mp4", TXT_MIME_MP4_AUDIO },
    { L"audio/x-m4a", TXT_MIME_MP4_AUDIO },
    { L"video/mpeg", TXT_MIME_MPEG_VIDEO },
    { L"video/mpeg1", TXT_MIME_MPEG_VIDEO },
    { L"video/mpeg2", TXT_MIME_MPEG_VIDEO },
    { L"video/mpg", TXT_MIME_MPEG_VIDEO },
    { L"video/mp4", TXT_MIME_MP4_VIDEO },
    { L"video/x-m4v", TXT_MIME_MP4_VIDEO },
    { L"image/vnd.dwg", TXT_MIME_DWG },
    { L"image/x-dwg", TXT_MIME_DWG },
    { L"application/acad", TXT_MIME_DWG },
    { L"application/x-acad", TXT_MIME_DWG },
    { L"application/edifact", TXT_MIME_EDIFACT },
    { L"application/edi-x12", TXT_MIME_EDI_X12 },
    { L"application/edi-consent", TXT_MIME_EDI_CONSENT },
    { L"text/isdoc", TXT_MIME_ISDOC },
    { L"application/x-isdoc", TXT_MIME_ISDOC },
    { L"text/isdocx", TXT_MIME_ISDOCX },
    { L"application/x-isdocx", TXT_MIME_ISDOCX },
    { L"application/vnd.software602.filler.form+xml", TXT_MIME_FO },
    { L"application/vnd.software602.filler.form-xml-zip", TXT_MIME_ZFO },
    { L"application/zip", TXT_MIME_ZIP },
    { L"application/x-compressed", TXT_MIME_ZIP },
    { L"application/x-zip-compressed", TXT_MIME_ZIP },
    { L"application/vnd.etsi.asic-e+zip", TXT_MIME_ASIC_E },
    { L"application/vnd.etsi.asic-s+zip", TXT_MIME_ASIC_S },
    { L"application/pkcs7-mime", TXT_MIME_PKCS7 },
    { L"application/x-pkcs7-mime", TXT_MIME_PKCS7 },
    { L"application/pkcs7-signature", TXT_MIME_PKCS7_SIGNATURE },
    { L"application/x-pkcs7-signature", TXT_MIME_PKCS7_SIGNATURE },
    { L"application/pkcs7-certificates", TXT_MIME_PKCS7_CERTIFICATES },
    { L"application/x-pkcs7-certificates", TXT_MIME_PKCS7_CERTIFICATES },
    { L"application/x-x509-ca-cert", TXT_MIME_X509 },
    { L"application/pkix-cert", TXT_MIME_X509 },
    { L"application/timestamp-reply", TXT_MIME_TIMESTAMP },
};

static constexpr const char* LegalTitleFields[] = {
    "dmLegalTitleLaw",
    "dmLegalTitleYear",
    "dmLegalTitleSect",
    "dmLegalTitlePar",
    "dmLegalTitlePoint"
};

static constexpr const char* ReportFieldKeys[] = {
    "dmID",
    "dmAnnotation",
    "dmAttachmentSize",
    "dmType",
    "dmAllowSubstDelivery",
    "dmVODZ",
    "attsNum",
    "dmSender",
    "dmSenderAddress",
    "dbIDSender",
    "dmSenderType",
    "dmSenderOrgUnit",
    "dmSenderOrgUnitNum",
    "userType",
    "dmUserType",
    "userName",
    "dmUserName",
    "pnFirstName",
    "pnMiddleName",
    "pnLastName",
    "pnLastNameAtBirth",
    "biDate",
    "biCity",
    "biCounty",
    "biState",
    "adCity",
    "adStreet",
    "adNumberInStreet",
    "adNumberInMunicipality",
    "adZipCode",
    "adState",
    "dmRecipient",
    "dmRecipientAddress",
    "dbIDRecipient",
    "dmRecipientType",
    "dmRecipientOrgUnit",
    "dmRecipientOrgUnitNum",
    "dmLegalTitle",
    "dmSenderRefNumber",
    "dmSenderIdent",
    "dmRecipientRefNumber",
    "dmRecipientIdent",
    "dmToHands",
    "dmPersonalDelivery",
    "dmDeliveryTime",
    "dmAcceptanceTime",
    "dmMessageStatus"
};

/// Measures the RichEdit's actual default character format, including its current DPI and zoom.
ReportFont GetReportFont(HWND edit) {
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    SendMessageW(edit, EM_GETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
    if (!(format.dwMask & CFM_FACE)
            || !(format.dwMask & CFM_SIZE)
            || !*format.szFaceName
            || format.yHeight <= 0) {
        throw std::runtime_error("saveFailed");
    }
    int numerator = 0;
    int denominator = 0;
    SendMessageW(edit, EM_GETZOOM, reinterpret_cast<WPARAM>(&numerator),
        reinterpret_cast<LPARAM>(&denominator));
    if (numerator <= 0 || denominator <= 0) {
        numerator = 1;
        denominator = 1;
    }
    int dpi = static_cast<int>(GetSystemDialogDpi(edit));
    ReportFont result;
    result.family = format.szFaceName;
    result.sizeTwips = MulDiv(format.yHeight, numerator, denominator);
    result.bold = (format.dwEffects & CFE_BOLD) != 0;
    result.italic = (format.dwEffects & CFE_ITALIC) != 0;
    result.underline = (format.dwEffects & CFE_UNDERLINE) != 0;
    result.strikeout = (format.dwEffects & CFE_STRIKEOUT) != 0;
    LOGFONTW definition{};
    definition.lfHeight = -MulDiv(result.sizeTwips, dpi, 1440);
    definition.lfWeight = result.bold ? FW_BOLD : FW_NORMAL;
    definition.lfItalic = result.italic;
    definition.lfCharSet = format.bCharSet;
    definition.lfPitchAndFamily = format.bPitchAndFamily;
    wcsncpy_s(definition.lfFaceName, format.szFaceName, _TRUNCATE);
    HFONT font = CreateFontIndirectW(&definition);
    HDC dc = font ? GetDC(edit) : nullptr;
    if (!dc) {
        DeleteObject(font);
        throw std::runtime_error("saveFailed");
    }
    HGDIOBJ previous = SelectObject(dc, font);
    TEXTMETRICW metrics{};
    BOOL measured = GetTextMetricsW(dc, &metrics);
    SelectObject(dc, previous);
    ReleaseDC(edit, dc);
    DeleteObject(font);
    if (!measured) {
        throw std::runtime_error("saveFailed");
    }
    result.linePixels = metrics.tmHeight;
    if (SendMessageW(edit, EM_GETLINECOUNT, 0, 0) > 1) {
        LONG second = static_cast<LONG>(SendMessageW(edit, EM_LINEINDEX, 1, 0));
        POINTL firstPosition{};
        POINTL secondPosition{};
        SendMessageW(edit, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&firstPosition), 0);
        SendMessageW(edit, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&secondPosition), second);
        if (secondPosition.y > firstPosition.y) {
            result.linePixels = secondPosition.y - firstPosition.y;
        }
    }
    result.lineTwips = MulDiv(result.linePixels, 1440, dpi);
    if (result.sizeTwips <= 0 || result.lineTwips <= 0) {
        throw std::runtime_error("saveFailed");
    }
    return result;
}

/// Formats document twips as locale-independent point values without unnecessary decimal zeros.
static std::wstring ReportPoints(LONG twips) {
    std::wstring result = std::to_wstring(twips / 20);
    int fraction = twips % 20 * 5;
    if (fraction) {
        result += L".";
        if (fraction < 10) {
            result += L"0";
        }
        result += std::to_wstring(fraction);
        if (result.back() == L'0') {
            result.pop_back();
        }
    }
    return result + L"pt";
}

/// Quotes a CSS font family while keeping HTML and XML style content safe without entity interpretation.
static std::wstring CssFontFamily(const std::wstring& family) {
    std::wstring result = L"\"";
    for (wchar_t character : family) {
        if (character < 0x20
                || character == 0x7F
                || character == L'\"'
                || character == L'\\'
                || character == L'<'
                || character == L'>'
                || character == L'&') {
            wchar_t escaped[12]{};
            int length = swprintf_s(escaped, L"\\%X ", static_cast<unsigned>(character));
            if (length < 0) {
                throw std::runtime_error("saveFailed");
            }
            result += escaped;
        } else {
            result += character;
        }
    }
    return result + L"\"";
}

/// Converts UTF-8 field names to the UTF-16 text used by native controls.
static std::wstring Wide(const std::string& text) {
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

/// Normalizes line endings to RichEdit carriage returns without duplicating CR/LF pairs.
static std::wstring ReportText(const std::wstring& value) {
    std::wstring result;
    result.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        wchar_t character = value[index];
        if (character == L'\n' && index > 0 && value[index - 1] == L'\r') {
            continue;
        }
        result += character == L'\n' ? L'\r' : character;
    }
    return result;
}

/// Honors the native high-contrast foreground instead of applying syntax or heading colors.
static bool UseHighlightingColors(bool enabled) {
    if (!enabled) {
        return false;
    }
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    BOOL available = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    return !available || !(contrast.dwFlags & HCF_HIGHCONTRASTON);
}

/// Records one nonempty token range in source order for both RichEdit and document writers.
static void AppendSourceSpan(std::vector<SourceSpan>& spans, size_t start, size_t end, SourceToken token) {
    if (start < end) {
        const CHARRANGE range{ static_cast<LONG>(start), static_cast<LONG>(end) };
        const SourceSpan span{ range, token };
        spans.push_back(span);
    }
}

/// Splits entity references from ordinary XML text or a quoted attribute value without overlapping runs.
static void AppendSourceText(std::vector<SourceSpan>& spans, std::wstring_view text, size_t start, size_t end,
    bool attribute) {
    size_t position = text.find(L'&', start);
    while (position < end) {
        size_t last = text.find_first_of(L";&< \t\r\n", position + 1);
        if (last < end && text[last] == L';') {
            if (attribute) {
                AppendSourceSpan(spans, start, position, SourceToken::Value);
            }
            AppendSourceSpan(spans, position, last + 1, SourceToken::Entity);
            start = last + 1;
            position = last;
        }
        position = text.find(L'&', position + 1);
    }
    if (attribute) {
        AppendSourceSpan(spans, start, end, SourceToken::Value);
    }
}

/// Distinguishes tag names, attribute names and quoted values while leaving delimiters unstyled.
static size_t AppendSourceTag(std::vector<SourceSpan>& spans, std::wstring_view text, size_t position) {
    ++position;
    if (position < text.size() && text[position] == L'/') {
        ++position;
    }
    size_t nameStart = position;
    size_t last = text.find_first_of(L" \t\r\n/?>=", position);
    position = last == std::wstring_view::npos ? text.size() : last;
    AppendSourceSpan(spans, nameStart, position, SourceToken::Tag);
    while (position < text.size()) {
        wchar_t character = text[position];
        if (character == L'>') {
            return position + 1;
        }
        if (character == L'"' || character == L'\'') {
            last = text.find(character, position + 1);
            size_t end = last == std::wstring_view::npos ? text.size() : last + 1;
            AppendSourceText(spans, text, position, end, true);
            position = end;
        } else if (character == L' '
                || character == L'\t'
                || character == L'\r'
                || character == L'\n'
                || character == L'='
                || character == L'/'
                || character == L'?'
                || character == L'<') {
            ++position;
        } else {
            last = text.find_first_of(L" \t\r\n/?>=\"'<", position);
            size_t end = last == std::wstring_view::npos ? text.size() : last;
            AppendSourceSpan(spans, position, end, SourceToken::Attribute);
            position = end;
        }
    }
    return position;
}

/// Finds the end of an XML declaration while respecting quotes and an internal DTD subset.
static size_t SourceDeclarationEnd(std::wstring_view text, size_t position) {
    wchar_t quote = 0;
    size_t brackets = 0;
    for (; position < text.size(); ++position) {
        wchar_t character = text[position];
        if (quote) {
            if (character == quote) {
                quote = 0;
            }
        } else if (character == L'"' || character == L'\'') {
            quote = character;
        } else if (text.substr(position).starts_with(L"<!--")) {
            size_t last = text.find(L"-->", position + 4);
            if (last == std::wstring_view::npos) {
                return text.size();
            }
            position = last + 2;
        } else if (character == L'[') {
            ++brackets;
        } else if (character == L']' && brackets) {
            --brackets;
        } else if (character == L'>' && !brackets) {
            return position + 1;
        }
    }
    return position;
}

/// Produces Firefox-style XML token ranges without altering whitespace or decoding entity references.
static std::vector<SourceSpan> SourceSpans(std::wstring_view text) {
    std::vector<SourceSpan> spans;
    size_t position = 0;
    while (position < text.size()) {
        SourceToken token;
        size_t end;
        if (text.substr(position).starts_with(L"<!--")) {
            size_t last = text.find(L"-->", position + 4);
            end = last == std::wstring_view::npos ? text.size() : last + 3;
            token = SourceToken::Comment;
        } else if (text.substr(position).starts_with(L"<![CDATA[")) {
            size_t last = text.find(L"]]>", position + 9);
            end = last == std::wstring_view::npos ? text.size() : last + 3;
            token = SourceToken::CData;
        } else if (text.substr(position).starts_with(L"<?")) {
            size_t last = text.find(L"?>", position + 2);
            end = last == std::wstring_view::npos ? text.size() : last + 2;
            token = SourceToken::ProcessingInstruction;
        } else if (text.substr(position).starts_with(L"<!")) {
            end = SourceDeclarationEnd(text, position + 2);
            token = SourceToken::Declaration;
        } else if (text[position] == L'<') {
            position = AppendSourceTag(spans, text, position);
            continue;
        } else {
            size_t last = text.find(L'<', position);
            end = last == std::wstring_view::npos ? text.size() : last;
            AppendSourceText(spans, text, position, end, false);
            position = end;
            continue;
        }
        AppendSourceSpan(spans, position, end, token);
        position = end;
    }
    return spans;
}

/// Writes one COLORREF in the hexadecimal RGB notation used by HTML and Office documents.
static std::wstring ExportColor(COLORREF color) {
    wchar_t text[7]{};
    swprintf_s(text, L"%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return text;
}

/// Selects a report section from the field's name and XML path, retaining unknown fields in supplementary
/// information.
static int SectionFor(const ZfoField& field) {
    std::string_view name = field.name;
    if (field.path.find(L"/dmEvent") != std::wstring::npos) {
        return 6;
    }
    if (field.path.find(L"/dmSenderInfo") != std::wstring::npos
            || name.starts_with("dmUser")
            || name == "userType"
            || name == "userName") {
        return 2;
    }
    if (name == "signature" || name == "signer" || name == "trust" || name == "dmQTimestamp") {
        return 7;
    }
    if (name == "dmID"
            || name == "dmAnnotation"
            || name == "dmAttachmentSize"
            || name == "dmType"
            || name == "dmAllowSubstDelivery"
            || name == "dmVODZ"
            || name == "attsNum") {
        return 0;
    }
    if (name == "dmSenderRefNumber"
            || name == "dmSenderIdent"
            || name == "dmRecipientRefNumber"
            || name == "dmRecipientIdent"
            || name == "dmToHands"
            || name == "dmPersonalDelivery") {
        return 4;
    }
    if (name.starts_with("dmSender") || name == "dbIDSender") {
        return 1;
    }
    if (name.starts_with("dmRecipient") || name == "dbIDRecipient") {
        return 3;
    }
    if (name == "dmDeliveryTime" || name == "dmAcceptanceTime" || name == "dmMessageStatus") {
        return 5;
    }
    return 4;
}

/// Preserves event and signature ordering and assigns known metadata the order used by Datovka.
static size_t ReportFieldOrder(const ZfoField& field) {
    int section = SectionFor(field);
    if (section != 6 && section != 7) {
        for (size_t index = 0; index < std::size(ReportFieldKeys); ++index) {
            if (field.name == ReportFieldKeys[index]) {
                return index;
            }
        }
    }
    return std::size(ReportFieldKeys);
}

/// Orders report metadata by section and known field order while keeping equal-priority fields stable.
static bool CompareReportFields(const ZfoField& left, const ZfoField& right) {
    int leftSection = SectionFor(left);
    int rightSection = SectionFor(right);
    if (leftSection != rightSection) {
        return leftSection < rightSection;
    }
    return ReportFieldOrder(left) < ReportFieldOrder(right);
}

/// Composes complete or partial legal references using Datovka's independent component conditions.
static std::wstring FormatLegalTitle(const std::array<std::wstring, std::size(LegalTitleFields)>& values,
        AppLanguage language) {
    std::wstring text;
    if (!values[0].empty() && values[0] != L"0") {
        text += values[0] + L"/";
    }
    if (!values[1].empty() && values[1] != L"0") {
        text += values[1] + L" Sb. ";
    }
    if (!values[2].empty()) {
        text += L"§ " + values[2];
    }
    if (!values[3].empty()) {
        text += L" " + std::wstring(GetLocalizedText(language, TXT_LEGAL_TITLE_PARAGRAPH)) + L" " + values[3];
    }
    if (!values[4].empty()) {
        text += L" " + std::wstring(GetLocalizedText(language, TXT_LEGAL_TITLE_LETTER)) + L" " + values[4];
    }
    return text;
}

/// Prepares all available metadata for native display and rich exports without changing the original XML.
static std::vector<ZfoField> ReportFields(const ZfoDocument& document, AppLanguage language) {
    std::vector<ZfoField> fields;
    std::array<std::wstring, std::size(LegalTitleFields)> legalTitle;
    bool hasLegalTitle = false;
    fields.reserve(document.fields.size());
    for (const ZfoField& field : document.fields) {
        size_t index = 0;
        while (index < std::size(LegalTitleFields) && field.name != LegalTitleFields[index]) {
            ++index;
        }
        if (index < std::size(LegalTitleFields)) {
            legalTitle[index] = field.value;
            hasLegalTitle = true;
        } else {
            fields.push_back(field);
        }
    }
    if (hasLegalTitle) {
        ZfoField field{ "dmLegalTitle", FormatLegalTitle(legalTitle, language) };
        fields.push_back(std::move(field));
    }
    std::stable_sort(fields.begin(), fields.end(), CompareReportFields);
    return fields;
}

/// Reads a fixed-width decimal component after its caller has validated the enclosing XML value length.
static bool ReadDateNumber(std::wstring_view text, size_t start, size_t count, WORD& number) {
    number = 0;
    for (size_t index = start; index < start + count; ++index) {
        wchar_t character = text[index];
        if (character < L'0' || character > L'9') {
            return false;
        }
        number = static_cast<WORD>(number * 10 + character - L'0');
    }
    return true;
}

/// Rejects invalid calendar dates and clock values before a native timezone conversion can normalize them.
static bool IsValidFieldDateTime(const SYSTEMTIME& value) {
    if (value.wHour > 23 || value.wMinute > 59 || value.wSecond > 59) {
        return false;
    }
    FILETIME fileTime{};
    SYSTEMTIME verified{};
    if (!SystemTimeToFileTime(&value, &fileTime) || !FileTimeToSystemTime(&fileTime, &verified)) {
        return false;
    }
    return value.wYear == verified.wYear && value.wMonth == verified.wMonth && value.wDay == verified.wDay;
}

/// Parses XML date and dateTime syntax, validating but omitting fractional seconds from the display value.
static bool ParseFieldDateTime(std::wstring_view text, FieldDateTime& date) {
    if (text.size() < 10 || text[4] != L'-' || text[7] != L'-') {
        return false;
    }
    bool valid = ReadDateNumber(text, 0, 4, date.value.wYear)
        && ReadDateNumber(text, 5, 2, date.value.wMonth)
        && ReadDateNumber(text, 8, 2, date.value.wDay);
    if (!valid) {
        return false;
    }
    size_t position = 10;
    if (position < text.size() && text[position] == L'T') {
        if (text.size() < 19 || text[13] != L':' || text[16] != L':') {
            return false;
        }
        valid = ReadDateNumber(text, 11, 2, date.value.wHour)
            && ReadDateNumber(text, 14, 2, date.value.wMinute)
            && ReadDateNumber(text, 17, 2, date.value.wSecond);
        if (!valid) {
            return false;
        }
        date.hasTime = true;
        position = 19;
        if (position < text.size() && text[position] == L'.') {
            size_t start = ++position;
            while (position < text.size() && text[position] >= L'0' && text[position] <= L'9') {
                ++position;
            }
            if (position == start) {
                return false;
            }
        }
    }
    if (!IsValidFieldDateTime(date.value)) {
        return false;
    }
    if (position == text.size()) {
        return true;
    }
    date.hasZone = true;
    std::wstring_view zone = text.substr(position);
    if (zone == L"Z") {
        return true;
    }
    if (zone.size() != 6 || zone[0] != L'+' && zone[0] != L'-' || zone[3] != L':') {
        return false;
    }
    WORD hour = 0;
    WORD minute = 0;
    valid = ReadDateNumber(zone, 1, 2, hour) && ReadDateNumber(zone, 4, 2, minute);
    if (!valid || hour > 14 || minute > 59 || hour == 14 && minute != 0) {
        return false;
    }
    date.offsetMinutes = hour * 60 + minute;
    if (zone[0] == L'-') {
        date.offsetMinutes = -date.offsetMinutes;
    }
    return true;
}

/// Converts an explicitly zoned timestamp using Windows' Prague/Bratislava timezone rules.
static bool ConvertFieldTimeToCentralEurope(FieldDateTime& date, bool& daylight) {
    TIME_ZONE_INFORMATION source{};
    source.Bias = -date.offsetMinutes;
    SYSTEMTIME utc{};
    if (!TzSpecificLocalTimeToSystemTime(&source, &date.value, &utc)) {
        return false;
    }
    RegistryTimeZone rules{};
    DWORD size = sizeof(rules);
    LSTATUS status = RegGetValueW(HKEY_LOCAL_MACHINE, CentralEuropeTimeZoneKey, L"TZI",
        RRF_RT_REG_BINARY, nullptr, &rules, &size);
    if (status != ERROR_SUCCESS || size != sizeof(rules)) {
        return false;
    }
    TIME_ZONE_INFORMATION central{};
    central.Bias = rules.bias;
    central.StandardBias = rules.standardBias;
    central.DaylightBias = rules.daylightBias;
    central.StandardDate = rules.standardDate;
    central.DaylightDate = rules.daylightDate;
    SYSTEMTIME local{};
    if (!SystemTimeToTzSpecificLocalTime(&central, &utc, &local)) {
        return false;
    }
    FILETIME utcFile{};
    FILETIME localFile{};
    if (!SystemTimeToFileTime(&utc, &utcFile) || !SystemTimeToFileTime(&local, &localFile)) {
        return false;
    }
    ULARGE_INTEGER utcTicks{};
    utcTicks.LowPart = utcFile.dwLowDateTime;
    utcTicks.HighPart = utcFile.dwHighDateTime;
    ULARGE_INTEGER localTicks{};
    localTicks.LowPart = localFile.dwLowDateTime;
    localTicks.HighPart = localFile.dwHighDateTime;
    LONGLONG offset = static_cast<LONGLONG>(localTicks.QuadPart) - static_cast<LONGLONG>(utcTicks.QuadPart);
    daylight = central.DaylightDate.wMonth != 0
        && offset == -static_cast<LONGLONG>(central.Bias + central.DaylightBias) * 600000000;
    date.value = local;
    return true;
}

/// Formats readable dates and minute-only times, with localized Central European timezone abbreviations.
static std::wstring LocalizedFieldDate(const std::wstring& text, AppLanguage language, bool dateOnly) {
    FieldDateTime date;
    if (!ParseFieldDateTime(text, date) || date.hasTime == dateOnly) {
        return text;
    }
    bool daylight = false;
    if (date.hasTime && date.hasZone && !ConvertFieldTimeToCentralEurope(date, daylight)) {
        return text;
    }
    const MessageDateFormat& format = MessageDateFormats[language];
    wchar_t dateText[128]{};
    int length = GetDateFormatEx(format.locale, 0, &date.value, format.date, dateText,
        ARRAYSIZE(dateText), nullptr);
    if (!length) {
        return text;
    }
    std::wstring result = dateText;
    if (date.hasTime) {
        wchar_t timeText[128]{};
        length = GetTimeFormatEx(format.locale, 0, &date.value, format.time, timeText,
            ARRAYSIZE(timeText));
        if (!length) {
            return text;
        }
        result += L" ";
        result += timeText;
        if (date.hasZone) {
            TextId zone = daylight ? TXT_CENTRAL_EUROPE_SUMMER_TIME : TXT_CENTRAL_EUROPE_TIME;
            result += L" ";
            result += GetLocalizedText(language, zone);
        }
    }
    return result;
}

/// Resolves a declared MIME type without changing its original spelling or guessing a format version.
static const wchar_t* MimeFormatName(std::wstring_view type, AppLanguage language) {
    type = type.substr(0, type.find(L';'));
    size_t first = type.find_first_not_of(L" \t\r\n");
    if (first == std::wstring_view::npos) {
        return GetLocalizedText(language, TXT_NOT_PROVIDED);
    }
    size_t last = type.find_last_not_of(L" \t\r\n");
    type = type.substr(first, last - first + 1);
    for (const MimeDescription& description : MimeDescriptions) {
        if (type.size() == description.type.size()
                && _wcsnicmp(type.data(), description.type.data(), type.size()) == 0) {
            return GetLocalizedText(language, description.name);
        }
    }
    return GetLocalizedText(language, TXT_MIME_UNKNOWN);
}

/// Prepares a human-readable content type followed by the original MIME type and other attachment metadata.
static std::vector<ZfoField> ReportAttachmentFields(const ZfoAttachment& attachment, AppLanguage language) {
    std::vector<ZfoField> fields;
    fields.reserve(attachment.fields.size() + 2);
    const ZfoField contentType{ "type", MimeFormatName(attachment.mimeType, language) };
    const ZfoField mimeType{ "dmMimeType", attachment.mimeType };
    fields.push_back(contentType);
    fields.push_back(mimeType);
    for (const ZfoField& field : attachment.fields) {
        if (field.name != "dmMimeType" && (field.name != "dmFileDescr" || field.value != attachment.name)) {
            fields.push_back(field);
        }
    }
    return fields;
}

/// Formats dates, known booleans and coded values while retaining unknown field values unchanged.
static std::wstring FieldValue(const ZfoField& field, const ZfoDocument& document, AppLanguage language) {
    if (field.value.empty()) {
        return GetLocalizedText(language, "notProvided");
    }
    for (const char* name : DateFields) {
        if (field.name == name) {
            return LocalizedFieldDate(field.value, language, field.name == "biDate");
        }
    }
    if (field.name == "dmPersonalDelivery"
            || field.name == "dmAllowSubstDelivery"
            || field.name == "dmVODZ") {
        const bool showProhibition = field.name == "dmAllowSubstDelivery";
        if (field.value == L"true" || field.value == L"1") {
            return GetLocalizedText(language, showProhibition ? "no" : "yes");
        }
        if (field.value == L"false" || field.value == L"0") {
            return GetLocalizedText(language, showProhibition ? "yes" : "no");
        }
    }
    if (field.name == "signature") {
        return GetLocalizedText(language, document.signatureValid ? "signatureValid" : "signatureInvalid");
    }
    if (field.name == "trust") {
        return GetLocalizedText(language, "trustInfo");
    }
    std::string prefix;
    if (field.name == "dmMessageStatus") {
        prefix = "state_";
    } else if (field.name == "dmSenderType" || field.name == "dmRecipientType") {
        prefix = "boxType_";
    } else if (field.name == "dmType") {
        prefix = "messageType_";
    } else if (field.name == "userType" || field.name == "dmUserType") {
        prefix = "userType_";
    } else if (field.name == "dmFileMetaType") {
        prefix = "fileRole_";
    }
    if (!prefix.empty()) {
        std::string code;
        for (wchar_t character : field.value) {
            if (character > 0x7F) {
                return field.value;
            }
            code.push_back(static_cast<char>(character));
        }
        const wchar_t* description = GetLocalizedText(language, prefix + code);
        if (*description) {
            return field.value + L" — " + description;
        }
    }
    if (field.name == "dmAttachmentSize") {
        return field.value + L" kB";
    }
    return field.value;
}

/// Resolves a metadata label using the same localization and XML-path fallback in every report format.
static std::wstring FieldLabel(const ZfoField& field, AppLanguage language) {
    const wchar_t* translation = GetLocalizedText(language, field.name);
    if (*translation) {
        return translation;
    }
    return field.path.empty() ? Wide(field.name) : field.path;
}

/// Escapes HTML and XML text content and attribute delimiters without interpreting message data as markup.
static std::wstring MarkupText(std::wstring_view text) {
    std::wstring result;
    result.reserve(text.size());
    for (wchar_t character : text) {
        switch (character) {
            case L'&':
                result += L"&amp;";
                break;
            case L'<':
                result += L"&lt;";
                break;
            case L'>':
                result += L"&gt;";
                break;
            case L'\"':
                result += L"&quot;";
                break;
            case L'\'':
                result += L"&#39;";
                break;
            default:
                result += character;
                break;
        }
    }
    return result;
}

/// Escapes source text and applies the same token ranges and character styles as the native XML view.
static std::wstring HtmlSource(const std::wstring& text, bool highlight) {
    const std::vector<SourceSpan> spans = highlight ? SourceSpans(text) : std::vector<SourceSpan>();
    std::wstring html;
    std::wstring_view source = text;
    size_t position = 0;
    for (const SourceSpan& span : spans) {
        size_t start = span.range.cpMin;
        size_t end = span.range.cpMax;
        const SourceStyle& style = SourceStyles[static_cast<size_t>(span.token)];
        html += MarkupText(source.substr(position, start - position));
        html += L"<span style=\"";
        if (style.color != CLR_DEFAULT) {
            html += L"color:#" + ExportColor(style.color) + L";";
        }
        if (style.effects & CFE_BOLD) {
            html += L"font-weight:bold;";
        }
        if (style.effects & CFE_ITALIC) {
            html += L"font-style:italic;";
        }
        html += L"\">" + MarkupText(source.substr(start, end - start)) + L"</span>";
        position = end;
    }
    return html + MarkupText(source.substr(position));
}

/// Returns the language tag used by exported document metadata and web roots.
static const wchar_t* DocumentLanguage(AppLanguage language) {
    if (language == LANG_CS) {
        return L"cs";
    }
    if (language == LANG_SK) {
        return L"sk";
    }
    return L"en-US";
}

/// Adds a localized metadata paragraph with the same value and binary-size unit as the visible report.
static void AppendReportField(std::vector<ReportParagraph>& paragraphs, const ZfoField& field,
        const ZfoDocument& document, AppLanguage language) {
    ReportParagraph paragraph;
    paragraph.label = FieldLabel(field, language) + L":";
    paragraph.text = L" " + FieldValue(field, document, language);
    if (field.binary >= 0) {
        paragraph.text += L" " + std::wstring(GetLocalizedText(language, "bytes"));
    }
    paragraphs.push_back(std::move(paragraph));
}

/// Collects message sections or source lines once for the web and Office document writers.
static std::vector<ReportParagraph> ReportParagraphs(const ZfoDocument& document, const std::wstring* source,
        AppLanguage language, bool highlight) {
    std::vector<ReportParagraph> paragraphs;
    if (source) {
        const std::vector<SourceSpan> spans = highlight ? SourceSpans(*source) : std::vector<SourceSpan>();
        size_t sourceStyle = 0;
        size_t start = 0;
        while (true) {
            size_t end = source->find_first_of(L"\r\n", start);
            ReportParagraph paragraph;
            size_t lineEnd = end == std::wstring::npos ? source->size() : end;
            paragraph.text = source->substr(start, lineEnd - start);
            while (sourceStyle < spans.size()
                    && static_cast<size_t>(spans[sourceStyle].range.cpMax) <= start) {
                ++sourceStyle;
            }
            for (size_t index = sourceStyle;
                index < spans.size() && static_cast<size_t>(spans[index].range.cpMin) < lineEnd; ++index) {
                SourceSpan span = spans[index];
                size_t spanStart = std::max(start, static_cast<size_t>(span.range.cpMin));
                size_t spanEnd = std::min(lineEnd, static_cast<size_t>(span.range.cpMax));
                span.range.cpMin = static_cast<LONG>(spanStart - start);
                span.range.cpMax = static_cast<LONG>(spanEnd - start);
                paragraph.sourceStyles.push_back(span);
            }
            paragraphs.push_back(std::move(paragraph));
            if (end == std::wstring::npos) {
                break;
            }
            if ((*source)[end] == L'\r' && end + 1 < source->size() && (*source)[end + 1] == L'\n') {
                ++end;
            }
            start = end + 1;
        }
        return paragraphs;
    }
    const std::vector<ZfoField> fields = ReportFields(document, language);
    for (int section = 0; section < static_cast<int>(std::size(SectionKeys)); ++section) {
        bool present = false;
        for (const ZfoField& field : fields) {
            if (SectionFor(field) == section) {
                present = true;
                break;
            }
        }
        if (present) {
            ReportParagraph heading;
            heading.text = GetLocalizedText(language, SectionKeys[section]);
            heading.heading = true;
            paragraphs.push_back(std::move(heading));
            for (const ZfoField& field : fields) {
                if (SectionFor(field) == section) {
                    AppendReportField(paragraphs, field, document, language);
                }
            }
        }
    }
    ReportParagraph heading;
    heading.text = GetLocalizedText(language, "attachments");
    heading.text += L" (" + std::to_wstring(document.attachments.size()) + L")";
    heading.heading = true;
    paragraphs.push_back(std::move(heading));
    if (document.attachments.empty()) {
        ReportParagraph paragraph;
        paragraph.text = GetLocalizedText(language, "noAttachments");
        paragraphs.push_back(std::move(paragraph));
    }
    for (const ZfoAttachment& attachment : document.attachments) {
        ReportParagraph paragraph;
        paragraph.label = attachment.name;
        paragraph.text = L" (" + std::to_wstring(attachment.data.size()) + L" ";
        paragraph.text += GetLocalizedText(language, "bytes");
        paragraph.text += L")";
        paragraph.attachment = true;
        paragraphs.push_back(std::move(paragraph));
        const std::vector<ZfoField> attachmentFields = ReportAttachmentFields(attachment, language);
        for (const ZfoField& field : attachmentFields) {
            AppendReportField(paragraphs, field, document, language);
        }
    }
    return paragraphs;
}

/// Preserves spaces, tabs and line breaks using OpenDocument text elements.
static std::wstring OdtText(std::wstring_view text) {
    std::wstring xml;
    size_t start = 0;
    for (size_t index = 0; index <= text.size(); ++index) {
        if (index < text.size()
                && text[index] != L' '
                && text[index] != L'\t'
                && text[index] != L'\r'
                && text[index] != L'\n') {
            continue;
        }
        xml += MarkupText(text.substr(start, index - start));
        if (index < text.size()) {
            if (text[index] == L' ') {
                size_t count = 1;
                while (index + 1 < text.size() && text[index + 1] == L' ') {
                    ++index;
                    ++count;
                }
                if (count == 1) {
                    xml += L"<text:s/>";
                } else {
                    xml += L"<text:s text:c=\"" + std::to_wstring(count) + L"\"/>";
                }
            } else if (text[index] == L'\t') {
                xml += L"<text:tab/>";
            } else {
                if (text[index] == L'\r' && index + 1 < text.size() && text[index + 1] == L'\n') {
                    ++index;
                }
                xml += L"<text:line-break/>";
            }
        }
        start = index + 1;
    }
    return xml;
}

/// Applies the captured font to an ODF paragraph style without relying on a reader's default font.
static std::wstring OdtFontProperties(const ReportFont& font, bool bold, COLORREF color = CLR_DEFAULT) {
    const std::wstring size = ReportPoints(font.sizeTwips);
    const std::wstring weight = bold ? L"bold" : L"normal";
    const std::wstring style = font.italic ? L"italic" : L"normal";
    std::wstring xml = L"      <style:text-properties style:font-name=\"ReportFont\" ";
    xml += L"style:font-name-asian=\"ReportFont\" style:font-name-complex=\"ReportFont\" ";
    xml += L"fo:font-size=\"" + size + L"\" style:font-size-asian=\"" + size + L"\" ";
    xml += L"style:font-size-complex=\"" + size + L"\" fo:font-weight=\"" + weight + L"\" ";
    xml += L"style:font-weight-asian=\"" + weight + L"\" style:font-weight-complex=\"" + weight + L"\" ";
    xml += L"fo:font-style=\"" + style + L"\" ";
    if (color != CLR_DEFAULT) {
        xml += L"fo:color=\"#" + ExportColor(color) + L"\" ";
    }
    if (font.underline) {
        xml += L"style:text-underline-style=\"solid\" ";
    }
    if (font.strikeout) {
        xml += L"style:text-line-through-style=\"solid\" ";
    }
    return xml + L"/>\r\n";
}

/// Defines source character styles without replacing the paragraph's selected font or point size.
static std::wstring OdtSourceStyles() {
    std::wstring xml;
    for (const SourceStyle& style : SourceStyles) {
        xml += L"    <style:style style:name=\"" + std::wstring(style.name) + L"\" style:family=\"text\">";
        xml += L"<style:text-properties";
        if (style.color != CLR_DEFAULT) {
            xml += L" fo:color=\"#" + ExportColor(style.color) + L"\"";
        }
        if (style.effects & CFE_BOLD) {
            xml += L" fo:font-weight=\"bold\" style:font-weight-asian=\"bold\" "
                L"style:font-weight-complex=\"bold\"";
        }
        if (style.effects & CFE_ITALIC) {
            xml += L" fo:font-style=\"italic\" style:font-style-asian=\"italic\" "
                L"style:font-style-complex=\"italic\"";
        }
        xml += L"/></style:style>\r\n";
    }
    return xml;
}

/// Serializes one source line using nonoverlapping character ranges and OpenDocument whitespace elements.
static std::wstring OdtSourceText(const ReportParagraph& paragraph) {
    std::wstring xml;
    std::wstring_view text = paragraph.text;
    size_t position = 0;
    for (const SourceSpan& span : paragraph.sourceStyles) {
        size_t start = span.range.cpMin;
        size_t end = span.range.cpMax;
        const SourceStyle& style = SourceStyles[static_cast<size_t>(span.token)];
        xml += OdtText(text.substr(position, start - position));
        xml += L"<text:span text:style-name=\"" + std::wstring(style.name) + L"\">";
        xml += OdtText(text.substr(start, end - start)) + L"</text:span>";
        position = end;
    }
    return xml + OdtText(text.substr(position));
}

/// Writes ODF 1.2 content and automatic styles using the actual RichEdit font and line height.
static std::wstring OdtContent(const std::vector<ReportParagraph>& paragraphs, const ReportFont& font,
    bool headingColors) {
    const std::wstring line = ReportPoints(font.lineTwips);
    std::wstring xml = DocumentXmlHeader;
    xml += L"<office:document-content xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\"\r\n"
        L"    xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\"\r\n    xmlns:style=\"urn:oasis:"
        L"names:tc:opendocument:xmlns:style:1.0\"\r\n    xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:"
        L"svg-compatible:1.0\"\r\n    xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1."
        L"0\" office:version=\"1.2\">\r\n  <office:font-face-decls>\r\n    <style:font-face "
        L"style:name=\"ReportFont\" svg:font-family=\"";
    xml += MarkupText(CssFontFamily(font.family)) + L"\"/>\r\n  </office:font-face-decls>\r\n";
    xml += L"  <office:automatic-styles>\r\n    <style:style style:name=\"Body\" style:family=\"paragraph\">"
        L"\r\n      <style:paragraph-properties fo:margin-top=\"0pt\" fo:margin-bottom=\"0pt\" "
        L"fo:line-height=\"";
    xml += line + L"\"/>\r\n" + OdtFontProperties(font, font.bold);
    xml += L"    </style:style>\r\n    <style:style style:name=\"Heading\" style:family=\"paragraph\">\r\n"
        L"      <style:paragraph-properties fo:margin-top=\"" + line + L"\" fo:margin-bottom=\"" + line;
    xml += L"\" fo:line-height=\"" + line + L"\" fo:keep-with-next=\"always\"/>\r\n";
    COLORREF headingColor = headingColors ? GetSysColor(COLOR_HOTLIGHT) : CLR_DEFAULT;
    xml += OdtFontProperties(font, true, headingColor);
    xml += L"    </style:style>\r\n    <style:style style:name=\"Label\" style:family=\"text\">\r\n"
        L"      <style:text-properties fo:font-weight=\"bold\" style:font-weight-asian=\"bold\" "
        L"style:font-weight-complex=\"bold\"/>\r\n    </style:style>\r\n";
    for (const ReportParagraph& paragraph : paragraphs) {
        if (!paragraph.sourceStyles.empty()) {
            xml += OdtSourceStyles();
            break;
        }
    }
    xml += L"  </office:automatic-styles>\r\n  <office:body>\r\n    <office:text>\r\n";
    for (const ReportParagraph& paragraph : paragraphs) {
        if (paragraph.heading) {
            xml += L"      <text:h text:style-name=\"Heading\" text:outline-level=\"1\">";
            xml += OdtText(paragraph.text) + L"</text:h>\r\n";
        } else {
            xml += L"      <text:p text:style-name=\"Body\">";
            if (!paragraph.label.empty()) {
                xml += L"<text:span text:style-name=\"Label\">" + OdtText(paragraph.label) + L"</text:span>";
            }
            xml += OdtSourceText(paragraph) + L"</text:p>\r\n";
        }
    }
    xml += L"    </office:text>\r\n  </office:body>\r\n</office:document-content>\r\n";
    return xml;
}

/// Preserves significant whitespace and character emphasis in one WordprocessingML run.
static std::wstring DocxRun(std::wstring_view text, bool bold, const SourceStyle* style = nullptr) {
    std::wstring xml = L"<w:r>";
    if (bold || style) {
        xml += L"<w:rPr>";
        if (bold || style && style->effects & CFE_BOLD) {
            xml += L"<w:b/><w:bCs/>";
        }
        if (style && style->effects & CFE_ITALIC) {
            xml += L"<w:i/><w:iCs/>";
        }
        if (style && style->color != CLR_DEFAULT) {
            xml += L"<w:color w:val=\"" + ExportColor(style->color) + L"\"/>";
        }
        xml += L"</w:rPr>";
    }
    size_t start = 0;
    for (size_t index = 0; index <= text.size(); ++index) {
        if (index < text.size() && text[index] != L'\t' && text[index] != L'\r' && text[index] != L'\n') {
            continue;
        }
        xml += L"<w:t xml:space=\"preserve\">" + MarkupText(text.substr(start, index - start)) + L"</w:t>";
        if (index < text.size()) {
            if (text[index] == L'\t') {
                xml += L"<w:tab/>";
            } else {
                if (text[index] == L'\r' && index + 1 < text.size() && text[index + 1] == L'\n') {
                    ++index;
                }
                xml += L"<w:br/>";
            }
        }
        start = index + 1;
    }
    xml += L"</w:r>";
    return xml;
}

/// Writes one XML source line with the same character colors and emphasis as RichEdit.
static std::wstring DocxSourceText(const ReportParagraph& paragraph) {
    std::wstring xml;
    std::wstring_view text = paragraph.text;
    size_t position = 0;
    for (const SourceSpan& span : paragraph.sourceStyles) {
        size_t start = span.range.cpMin;
        size_t end = span.range.cpMax;
        const SourceStyle& style = SourceStyles[static_cast<size_t>(span.token)];
        xml += DocxRun(text.substr(position, start - position), false);
        xml += DocxRun(text.substr(start, end - start), false, &style);
        position = end;
    }
    return xml + DocxRun(text.substr(position), false);
}

/// Writes the report paragraphs as a native WordprocessingML document with heading and label styles.
static std::wstring DocxContent(const std::vector<ReportParagraph>& paragraphs) {
    std::wstring xml = DocumentXmlHeader;
    xml += L"<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\r\n"
        L"  <w:body>\r\n";
    for (const ReportParagraph& paragraph : paragraphs) {
        xml += L"    <w:p>";
        if (paragraph.heading) {
            xml += L"<w:pPr><w:pStyle w:val=\"Heading1\"/></w:pPr>";
        }
        if (!paragraph.label.empty()) {
            xml += DocxRun(paragraph.label, true);
        }
        xml += DocxSourceText(paragraph) + L"</w:p>\r\n";
    }
    xml += L"  </w:body>\r\n</w:document>\r\n";
    return xml;
}

/// Encodes one package part as validated UTF-8 without a byte-order mark.
static void AppendDocumentPart(std::vector<DocumentPart>& parts, const char* name, const std::wstring& text) {
    if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("saveFailed");
    }
    int length = static_cast<int>(text.size());
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length,
        nullptr, 0, nullptr, nullptr);
    if (!size && length) {
        throw std::runtime_error("saveFailed");
    }
    DocumentPart part;
    part.name = name;
    part.data.resize(size);
    if (size) {
        int copied = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length,
            reinterpret_cast<char*>(part.data.data()), size, nullptr, nullptr);
        if (copied != size) {
            throw std::runtime_error("saveFailed");
        }
    }
    parts.push_back(std::move(part));
}

/// Calculates the standard ZIP CRC-32 over the uncompressed bytes of a package part.
static std::uint32_t DocumentCrc(const std::vector<std::uint8_t>& data) {
    std::uint32_t crc = 0xFFFFFFFF;
    for (std::uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = crc & 1 ? crc >> 1 ^ 0xEDB88320 : crc >> 1;
        }
    }
    return ~crc;
}

/// Appends a packed ZIP record using the Windows platform's little-endian integer representation.
template<typename T>
static void AppendZipRecord(std::vector<std::uint8_t>& bytes, const T& record) {
    const std::uint8_t* data = reinterpret_cast<const std::uint8_t*>(&record);
    bytes.insert(bytes.end(), data, data + sizeof(record));
}

/// Writes a complete stored ZIP package shared by ODT and DOCX, preserving the supplied part order.
static std::vector<std::uint8_t> DocumentPackage(std::vector<DocumentPart>& parts) {
    std::uint64_t total = sizeof(ZipDirectoryEnd);
    for (const DocumentPart& part : parts) {
        total += sizeof(ZipEntryHeader) + sizeof(ZipDirectoryHeader)
            + 2ULL * part.name.size() + part.data.size();
        if (part.name.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error("saveFailed");
        }
    }
    if (total > std::numeric_limits<std::uint32_t>::max()
            || parts.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("saveFailed");
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(static_cast<size_t>(total));
    for (DocumentPart& part : parts) {
        part.crc = DocumentCrc(part.data);
        part.offset = static_cast<std::uint32_t>(bytes.size());
        ZipEntryHeader header;
        header.crc = part.crc;
        header.compressedSize = static_cast<std::uint32_t>(part.data.size());
        header.size = header.compressedSize;
        header.nameLength = static_cast<std::uint16_t>(part.name.size());
        AppendZipRecord(bytes, header);
        bytes.insert(bytes.end(), part.name.begin(), part.name.end());
        bytes.insert(bytes.end(), part.data.begin(), part.data.end());
    }
    std::uint32_t directory = static_cast<std::uint32_t>(bytes.size());
    for (const DocumentPart& part : parts) {
        ZipDirectoryHeader header;
        header.crc = part.crc;
        header.compressedSize = static_cast<std::uint32_t>(part.data.size());
        header.size = header.compressedSize;
        header.nameLength = static_cast<std::uint16_t>(part.name.size());
        header.offset = part.offset;
        AppendZipRecord(bytes, header);
        bytes.insert(bytes.end(), part.name.begin(), part.name.end());
    }
    ZipDirectoryEnd end;
    end.diskEntries = static_cast<std::uint16_t>(parts.size());
    end.entries = end.diskEntries;
    end.size = static_cast<std::uint32_t>(bytes.size()) - directory;
    end.offset = directory;
    AppendZipRecord(bytes, end);
    return bytes;
}

/// Appends emphasized text and records its UTF-16 range for the report's bold formatting.
static void AppendEmphasized(std::wstring& text, std::vector<CHARRANGE>& bold, const std::wstring& value) {
    LONG start = static_cast<LONG>(text.size());
    text += value;
    bold.push_back(CHARRANGE{ start, static_cast<LONG>(text.size()) });
}

/// Appends a section heading with the report's existing paragraph spacing.
static void AppendHeading(std::wstring& text, std::vector<CHARRANGE>& bold, std::vector<CHARRANGE>& headings,
        const std::wstring& value) {
    if (!text.empty()) {
        text += L"\r";
    }
    AppendEmphasized(text, bold, value);
    headings.push_back(bold.back());
    text += L"\r\r";
}

/// Appends a localized metadata field and records any downloadable binary link.
static void AppendField(std::wstring& text, std::vector<CHARRANGE>& bold, std::vector<AttachmentLink>& links,
        const ZfoField& item, const ZfoDocument& document, AppLanguage language) {
    std::wstring label = FieldLabel(item, language);
    AppendEmphasized(text, bold, label + L":");
    text += L" " + ReportText(FieldValue(item, document, language));
    if (item.binary >= 0) {
        text += L" " + std::wstring(GetLocalizedText(language, "bytes")) + L"  ";
        LONG start = static_cast<LONG>(text.size());
        text += GetLocalizedText(language, "saveLink");
        const CHARRANGE range{ start, static_cast<LONG>(text.size()) };
        const AttachmentLink link{ range, item.binary, true, true };
        links.push_back(link);
    }
    text += L"\r";
}

/// Creates the read-only RichEdit report and enables attachment links and selection notifications.
bool MessageView::Create(HWND owner, HINSTANCE instance) {
    window = CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VSCROLL
        | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_NOOLEDRAGDROP, 0, 0, 1, 1, owner,
        reinterpret_cast<HMENU>(ID_MAIN_REPORT), instance, nullptr);
    if (window) {
        SendMessageW(window, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
        SendMessageW(window, EM_SETEVENTMASK, 0, ENM_LINK | ENM_SELCHANGE);
        SendMessageW(window, EM_AUTOURLDETECT, FALSE, 0);
        SendMessageW(window, EM_SETUNDOLIMIT, 0, 0);
        SendMessageW(window, EM_SETBKGNDCOLOR, TRUE, 0);
    }
    return window != nullptr;
}

/// Returns the native RichEdit report control.
HWND MessageView::GetWindow() const {
    return window;
}

/// Builds localized message sections, formats their headings and records clickable attachment and binary
/// links.
void MessageView::Display(const ZfoDocument* document, AppLanguage language, bool highlight) {
    links.clear();
    std::wstring text;
    std::vector<CHARRANGE> bold;
    std::vector<CHARRANGE> headings;
    if (document) {
        const std::vector<ZfoField> fields = ReportFields(*document, language);
        for (int section = 0; section < static_cast<int>(std::size(SectionKeys)); ++section) {
            bool present = false;
            for (const ZfoField& item : fields) {
                if (SectionFor(item) == section) {
                    present = true;
                    break;
                }
            }
            if (!present) {
                continue;
            }
            AppendHeading(text, bold, headings, GetLocalizedText(language, SectionKeys[section]));
            for (const ZfoField& item : fields) {
                if (SectionFor(item) == section) {
                    AppendField(text, bold, links, item, *document, language);
                }
            }
        }
        std::wstring attachmentHeading = GetLocalizedText(language, "attachments");
        attachmentHeading += L" (" + std::to_wstring(document->attachments.size()) + L")";
        AppendHeading(text, bold, headings, attachmentHeading);
        if (document->attachments.empty()) {
            text += GetLocalizedText(language, "noAttachments");
        }
        for (int index = 0; index < static_cast<int>(document->attachments.size()); ++index) {
            const ZfoAttachment& attachment = document->attachments[index];
            LONG start = static_cast<LONG>(text.size());
            text += attachment.name;
            const CHARRANGE openRange{ start, static_cast<LONG>(text.size()) };
            const AttachmentLink openLink{ openRange, index, false };
            links.push_back(openLink);
            text += L" (" + std::to_wstring(attachment.data.size()) + L" "
                + GetLocalizedText(language, "bytes") + L")  ";
            start = static_cast<LONG>(text.size());
            text += GetLocalizedText(language, "saveLink");
            const CHARRANGE saveRange{ start, static_cast<LONG>(text.size()) };
            const AttachmentLink saveLink{ saveRange, index, true };
            links.push_back(saveLink);
            text += L"\r";
            const std::vector<ZfoField> attachmentFields = ReportAttachmentFields(attachment, language);
            for (const ZfoField& item : attachmentFields) {
                AppendField(text, bold, links, item, *document, language);
            }
            text += L"\r";
        }
    }
    SendMessageW(window, WM_SETREDRAW, FALSE, 0);
    SetWindowTextW(window, text.c_str());
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    SendMessageW(window, EM_GETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
    format.dwMask = CFM_BOLD | CFM_UNDERLINE | CFM_LINK | CFM_COLOR;
    format.dwEffects = CFE_AUTOCOLOR | format.dwEffects & (CFE_BOLD | CFE_UNDERLINE);
    SendMessageW(window, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
    format.dwMask = CFM_BOLD;
    format.dwEffects = CFE_BOLD;
    for (const CHARRANGE& range : bold) {
        SendMessageW(window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
        SendMessageW(window, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    }
    if (UseHighlightingColors(highlight)) {
        format.dwMask = CFM_COLOR;
        format.dwEffects = 0;
        format.crTextColor = GetSysColor(COLOR_HOTLIGHT);
        for (const CHARRANGE& range : headings) {
            SendMessageW(window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
            SendMessageW(window, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
        }
    }
    format.dwMask = CFM_LINK | CFM_COLOR | CFM_UNDERLINE;
    format.dwEffects = CFE_LINK | CFE_UNDERLINE;
    format.crTextColor = GetSysColor(COLOR_HOTLIGHT);
    for (const AttachmentLink& link : links) {
        SendMessageW(window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&link.range));
        SendMessageW(window, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    }
    CHARRANGE selection{};
    SendMessageW(window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    SendMessageW(window, WM_VSCROLL, SB_TOP, 0);
    SendMessageW(window, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(window, nullptr, TRUE);
}

/// Loads formatted XML and colors its syntax without changing the selected font or source text.
void MessageView::DisplaySource(HWND edit, const std::wstring& text, bool highlight) {
    const std::wstring normalized = ReportText(text);
    const std::vector<SourceSpan> spans = UseHighlightingColors(highlight)
        ? SourceSpans(normalized)
        : std::vector<SourceSpan>();
    bool visible = GetWindowLongPtrW(edit, GWL_STYLE) & WS_VISIBLE;
    LRESULT events = SendMessageW(edit, EM_GETEVENTMASK, 0, 0);
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    SendMessageW(edit, EM_GETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
    DWORD baseEffects = format.dwEffects & (CFE_BOLD | CFE_ITALIC);
    SendMessageW(edit, EM_SETEVENTMASK, 0, 0);
    if (visible) {
        SendMessageW(edit, WM_SETREDRAW, FALSE, 0);
    }
    SetWindowTextW(edit, normalized.c_str());
    format.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC;
    format.dwEffects = CFE_AUTOCOLOR | baseEffects;
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
    for (const SourceSpan& span : spans) {
        const SourceStyle& style = SourceStyles[static_cast<size_t>(span.token)];
        format.dwEffects = baseEffects | style.effects;
        if (style.color == CLR_DEFAULT) {
            format.dwEffects |= CFE_AUTOCOLOR;
        }
        format.crTextColor = style.color;
        SendMessageW(edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&span.range));
        SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    }
    CHARRANGE selection{};
    POINT scroll{};
    SendMessageW(edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    SendMessageW(edit, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    SendMessageW(edit, EM_SETEVENTMASK, 0, events);
    if (visible) {
        SendMessageW(edit, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(edit, nullptr, TRUE);
    }
}

/// Formats the localized message or XML source as UTF-8-ready HTML 4.01 Strict or XHTML 1.1 markup.
std::wstring MessageView::FormatHtml(HWND edit, const ZfoDocument& document, const std::wstring* source,
        const std::wstring& title, AppLanguage language, bool xhtml, bool highlight) {
    ReportFont font = GetReportFont(edit);
    highlight = UseHighlightingColors(highlight);
    const wchar_t* languageTag = DocumentLanguage(language);
    std::wstring html;
    if (xhtml) {
        html = L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML "
            L"1.1//EN\"\r\n    \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\r\n<html xmlns=\"http://www."
            L"w3.org/1999/xhtml\" xml:lang=\"";
    } else {
        html = L"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\r\n    \"http://www.w3.org/TR/html4/"
            L"strict.dtd\">\r\n<html lang=\"";
    }
    html += languageTag;
    html += L"\">\r\n<head>\r\n    <title>" + MarkupText(title) + L"</title>\r\n";
    html += L"    <meta http-equiv=\"Content-Type\" content=\"";
    html += xhtml ? L"application/xhtml+xml" : L"text/html";
    html += L"; charset=utf-8\"";
    html += xhtml ? L" />\r\n" : L">\r\n";
    html += L"    <meta name=\"generator\" content=\"ZFO Viewer\"";
    html += xhtml ? L" />\r\n" : L">\r\n";
    html += L"    <style type=\"text/css\">\r\n        body { font-family: "
        + CssFontFamily(font.family);
    html += L"; font-size: " + ReportPoints(font.sizeTwips) + L"; line-height: "
        + ReportPoints(font.lineTwips);
    html += font.bold ? L"; font-weight: bold" : L"; font-weight: normal";
    html += font.italic ? L"; font-style: italic" : L"; font-style: normal";
    if (font.underline || font.strikeout) {
        html += L"; text-decoration:";
        if (font.underline) {
            html += L" underline";
        }
        if (font.strikeout) {
            html += L" line-through";
        }
    }
    const std::wstring line = ReportPoints(font.lineTwips);
    html += L"; }\r\n        h2 { font-size: 1em; margin: " + line + L" 0 " + line + L"; }\r\n";
    if (!source && highlight) {
        html += L"        h2 { color: #" + ExportColor(GetSysColor(COLOR_HOTLIGHT)) + L"; }\r\n";
    }
    html += L"        h2:first-child { margin-top: 0; }\r\n        p { margin: 0; }\r\n        .attachment { "
        L"margin-top: " + line + L"; }\r\n";
    html += L"        pre { font: inherit; margin: 0; }\r\n        .value, pre { white-space: pre-wrap; "
        L"word-wrap: break-word; }\r\n    </style>\r\n</head>\r\n<body>\r\n";
    if (source) {
        html += L"    <pre>" + HtmlSource(*source, highlight) + L"</pre>\r\n";
    } else {
        const std::vector<ReportParagraph> paragraphs =
            ReportParagraphs(document, nullptr, language, highlight);
        for (const ReportParagraph& paragraph : paragraphs) {
            if (paragraph.heading) {
                html += L"    <h2>" + MarkupText(paragraph.text) + L"</h2>\r\n";
            } else {
                html += paragraph.attachment ? L"    <p class=\"attachment\">" : L"    <p>";
                if (!paragraph.label.empty()) {
                    html += L"<strong>" + MarkupText(paragraph.label) + L"</strong>";
                }
                html += L"<span class=\"value\">" + MarkupText(paragraph.text) + L"</span></p>\r\n";
            }
        }
    }
    html += L"</body>\r\n</html>\r\n";
    return html;
}

/// Builds a standalone OpenDocument Text or Office Open XML package without an installed Office suite.
std::vector<std::uint8_t> MessageView::FormatDocument(HWND edit, const ZfoDocument& document,
        const std::wstring* source, const std::wstring& title, AppLanguage language, bool docx,
        bool highlight) {
    ReportFont font = GetReportFont(edit);
    highlight = UseHighlightingColors(highlight);
    const std::vector<ReportParagraph> paragraphs = ReportParagraphs(document, source, language, highlight);
    std::vector<DocumentPart> parts;
    const std::wstring languageTag = DocumentLanguage(language);
    if (docx) {
        std::wstring contentTypes = DocumentXmlHeader;
        contentTypes += L"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\r\n"
            L"  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package."
            L"relationships+xml\"/>\r\n  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\r\n"
            L"  <Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-"
            L"officedocument.wordprocessingml.document.main+xml\"/>\r\n  <Override "
            L"PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument."
            L"wordprocessingml.styles+xml\"/>\r\n  <Override PartName=\"/docProps/core.xml\" "
            L"ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>\r\n</Types>\r\n";
        AppendDocumentPart(parts, "[Content_Types].xml", contentTypes);
        std::wstring relationships = DocumentXmlHeader;
        relationships += L"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/"
            L"relationships\">\r\n  <Relationship Id=\"document\" Type=\"http://schemas.openxmlformats.org/"
            L"officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>\r\n"
            L"  <Relationship Id=\"metadata\" Type=\"http://schemas.openxmlformats.org/package/2006/"
            L"relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>\r\n"
            L"</Relationships>\r\n";
        AppendDocumentPart(parts, "_rels/.rels", relationships);
        AppendDocumentPart(parts, "word/document.xml", DocxContent(paragraphs));
        relationships = DocumentXmlHeader;
        relationships += L"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/"
            L"relationships\">\r\n  <Relationship Id=\"styles\" Type=\"http://schemas.openxmlformats.org/"
            L"officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\r\n</Relationships>\r\n";
        AppendDocumentPart(parts, "word/_rels/document.xml.rels", relationships);
        const std::wstring family = MarkupText(font.family);
        const std::wstring halfPoints = std::to_wstring((font.sizeTwips + 5) / 10);
        const std::wstring line = std::to_wstring(font.lineTwips);
        std::wstring styles = DocumentXmlHeader;
        styles += L"<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\r\n "
            L" <w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii=\"" + family + L"\" w:hAnsi=\"" + family;
        styles += L"\" w:eastAsia=\"" + family + L"\" w:cs=\"" + family + L"\"/>";
        styles += font.bold ? L"<w:b/>" : L"<w:b w:val=\"0\"/>";
        styles += font.italic ? L"<w:i/><w:iCs/>" : L"<w:i w:val=\"0\"/><w:iCs w:val=\"0\"/>";
        if (font.underline) {
            styles += L"<w:u w:val=\"single\"/>";
        }
        if (font.strikeout) {
            styles += L"<w:strike/>";
        }
        styles += L"<w:sz w:val=\"" + halfPoints + L"\"/><w:szCs w:val=\"" + halfPoints + L"\"/>";
        styles += L"<w:lang w:val=\"" + languageTag + L"\"/></w:rPr></w:rPrDefault></w:docDefaults>\r\n"
            L"  <w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\">\r\n    <w:name "
            L"w:val=\"Normal\"/>\r\n    <w:pPr><w:spacing w:before=\"0\" w:after=\"0\" w:line=\"" + line;
        styles += L"\" w:lineRule=\"exact\"/></w:pPr>\r\n  </w:style>\r\n  <w:style w:type=\"paragraph\" "
            L"w:styleId=\"Heading1\">\r\n    <w:name w:val=\"Heading 1\"/><w:basedOn w:val=\"Normal\"/>"
            L"<w:next w:val=\"Normal\"/>\r\n    <w:pPr><w:keepNext/><w:spacing "
            L"w:before=\"" + line + L"\" w:after=\"" + line;
        styles += L"\"/><w:outlineLvl w:val=\"0\"/></w:pPr>\r\n    <w:rPr><w:b/><w:bCs/>";
        if (!source && highlight) {
            styles += L"<w:color w:val=\"" + ExportColor(GetSysColor(COLOR_HOTLIGHT)) + L"\"/>";
        }
        styles += L"</w:rPr>\r\n  </w:style>\r\n</w:styles>\r\n";
        AppendDocumentPart(parts, "word/styles.xml", styles);
        std::wstring metadata = DocumentXmlHeader;
        metadata += L"<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/"
            L"core-properties\"\r\n    xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\r\n"
            L"  <dc:title>" + MarkupText(title) + L"</dc:title>\r\n";
        metadata += L"  <dc:language>" + languageTag + L"</dc:language>\r\n</cp:coreProperties>\r\n";
        AppendDocumentPart(parts, "docProps/core.xml", metadata);
    } else {
        AppendDocumentPart(parts, "mimetype", L"application/vnd.oasis.opendocument.text");
        AppendDocumentPart(parts, "content.xml", OdtContent(paragraphs, font, !source && highlight));
        std::wstring manifest = DocumentXmlHeader;
        manifest += L"<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1."
            L"0\" manifest:version=\"1.2\">\r\n  <manifest:file-entry manifest:full-path=\"/\" "
            L"manifest:media-type=\"application/vnd.oasis.opendocument.text\" manifest:version=\"1.2\"/>\r\n"
            L"  <manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>"
            L"\r\n  <manifest:file-entry manifest:full-path=\"meta.xml\" manifest:media-type=\"text/xml\"/>"
            L"\r\n</manifest:manifest>\r\n";
        AppendDocumentPart(parts, "META-INF/manifest.xml", manifest);
        std::wstring metadata = DocumentXmlHeader;
        metadata += L"<office:document-meta xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1."
            L"0\"\r\n    xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\"\r\n"
            L"    xmlns:dc=\"http://purl.org/dc/elements/1.1/\" office:version=\"1.2\">\r\n  <office:meta>"
            L"<meta:generator>ZFO Viewer</meta:generator>\r\n"
            L"    <dc:title>" + MarkupText(title) + L"</dc:title>\r\n";
        metadata += L"    <dc:language>" + languageTag + L"</dc:language>\r\n  </office:meta>\r\n"
            L"</office:document-meta>\r\n";
        AppendDocumentPart(parts, "meta.xml", metadata);
    }
    return DocumentPackage(parts);
}

/// Returns the attachment or binary-data link covering the specified UTF-16 character position.
const AttachmentLink* MessageView::FindLink(LONG position) const {
    for (const AttachmentLink& link : links) {
        if (position >= link.range.cpMin && position < link.range.cpMax) {
            return &link;
        }
    }
    return nullptr;
}

/// Returns the attachment under the selection start, or -1 for other text and binary-metadata links.
int MessageView::SelectedAttachment() const {
    CHARRANGE range{};
    SendMessageW(window, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
    const AttachmentLink* link = FindLink(range.cpMin);
    if (link && !link->binary) {
        return link->attachment;
    }
    return -1;
}
