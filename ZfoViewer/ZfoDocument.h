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

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

/// Preserves one XML field or attribute with its localization key, value, path and optional binary-data
/// index.
struct ZfoField {
    std::string name;
    std::wstring value;
    std::wstring path;
    int binary = -1;
};

/// Owns decoded attachment bytes, its filename and content type, and all associated metadata fields.
struct ZfoAttachment {
    std::wstring name;
    std::wstring mimeType;
    std::vector<std::uint8_t> data;

    /// All collected message fields, decoded attachments and binary metadata payloads.
    std::vector<ZfoField> fields;
};

/// Owns decoded message XML, metadata, attachments and the result of the offline signature check.
class ZfoDocument {

public:
    /// Message identifiers used for document naming and reporting.
    std::wstring subject;
    std::wstring identifier;

    /// Original ZFO bytes retained for exact copies independently of the source file.
    std::vector<std::uint8_t> zfoBytes;

    /// Original XML bytes retained for formatting and export.
    std::vector<std::uint8_t> xmlBytes;

    /// All collected message fields, decoded attachments and binary metadata payloads.
    std::vector<ZfoField> fields;
    std::vector<ZfoAttachment> attachments;
    std::vector<ZfoAttachment> binaryFields;

    /// Cryptographic signature result; this does not establish certificate trust or revocation status.
    bool signatureValid = false;

    /// Formats a UTF-8 XML copy while preserving mixed content, xml:space, CDATA and explicit carriage
    /// returns.
    std::vector<std::uint8_t> FormattedXml() const;

    /// Writes the formatted XML copy to the selected export path.
    void ExportXml(const std::filesystem::path& path) const;

    /// Decodes a CMS data message, records its signature check and extracts XML fields and embedded
    /// attachment bytes.
    static ZfoDocument Load(const std::filesystem::path& path);

    /// Removes directory components and replaces unsafe Windows filename characters and reserved device
    /// names.
    static std::wstring SafeFileName(const std::wstring& name);

    /// Writes decoded bytes unchanged and reports open, write or flush failures.
    static void Save(const std::filesystem::path& path, const std::vector<std::uint8_t>& data);
};
