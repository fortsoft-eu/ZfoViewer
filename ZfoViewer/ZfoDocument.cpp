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

#include "ZfoDocument.h"
#include <windows.h>
#include <wincrypt.h>
#include <msxml6.h>
#include <xmllite.h>
#include <comdef.h>
#include <wrl/client.h>
#include <algorithm>
#include <fstream>
#include <memory>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

/// Tracks inherited whitespace preservation and mixed content while formatting each XML element.
struct XmlFormatting {
    bool preserveSpace = false;
    bool inlineContent = false;
    bool hasMarkup = false;
    bool hasWhitespace = false;
};

/// Converts a failed COM result into the localized error key supplied by the caller.
static void Check(HRESULT result, const char* message) {
    if (FAILED(result)) {
        throw std::runtime_error(message);
    }
}

/// Records mixed-content, whitespace and inherited xml:space requirements before XML indentation is applied.
static std::vector<XmlFormatting> ReadXmlFormatting(IXmlReader* reader) {
    std::vector<XmlFormatting> elements;
    std::vector<size_t> parents;
    while (true) {
        XmlNodeType type{};
        HRESULT result = reader->Read(&type);
        if (result == S_FALSE) {
            break;
        }
        Check(result, "saveFailed");
        if (type == XmlNodeType_Element) {
            XmlFormatting element;
            if (!parents.empty()) {
                XmlFormatting& parent = elements[parents.back()];
                parent.hasMarkup = true;
                element.preserveSpace = parent.preserveSpace;
            }
            result = reader->MoveToAttributeByName(L"space", L"http://www.w3.org/XML/1998/namespace");
            Check(result, "saveFailed");
            if (result == S_OK) {
                const wchar_t* value = nullptr;
                Check(reader->GetValue(&value, nullptr), "saveFailed");
                element.preserveSpace = value && wcscmp(value, L"preserve") == 0;
                Check(reader->MoveToElement(), "saveFailed");
            }
            element.inlineContent = element.preserveSpace;
            elements.push_back(element);
            if (!reader->IsEmptyElement()) {
                parents.push_back(elements.size() - 1);
            }
        } else if (type == XmlNodeType_EndElement) {
            XmlFormatting& element = elements[parents.back()];
            element.inlineContent = element.inlineContent || element.hasWhitespace && !element.hasMarkup;
            parents.pop_back();
        } else if (!parents.empty()) {
            XmlFormatting& element = elements[parents.back()];
            if (type == XmlNodeType_Text || type == XmlNodeType_CDATA) {
                element.inlineContent = true;
            } else if (type == XmlNodeType_Whitespace) {
                element.hasWhitespace = true;
            } else if (type == XmlNodeType_Comment || type == XmlNodeType_ProcessingInstruction) {
                element.hasMarkup = true;
            }
        }
    }
    return elements;
}

/// Copies a DOM node's text to an owned UTF-16 string, returning empty text for a missing node.
static std::wstring Text(IXMLDOMNode* node) {
    if (!node) {
        return {};
    }
    BSTR text = nullptr;
    Check(node->get_text(&text), "invalidXml");
    _bstr_t value(text, false);
    return value.length() ? std::wstring(static_cast<const wchar_t*>(value), value.length()) : L"";
}

/// Evaluates one XPath expression and returns its first matching DOM node.
static ComPtr<IXMLDOMNode> Select(IXMLDOMNode* node, const std::wstring& query) {
    ComPtr<IXMLDOMNode> selected;
    Check(node->selectSingleNode(_bstr_t(query.c_str()), &selected), "invalidXml");
    return selected;
}

/// Returns the first descendant field matching the requested local element name.
static std::wstring Value(IXMLDOMNode* node, const wchar_t* name) {
    return Text(Select(node, L".//*[local-name()='" + std::wstring(name) + L"']").Get());
}

/// Returns the requested DOM attribute value, or empty text when it is absent.
static std::wstring Attribute(IXMLDOMNode* node, const wchar_t* name) {
    return Text(Select(node, L"@" + std::wstring(name)).Get());
}

/// Returns a DOM node's local name independently of its namespace prefix.
static std::wstring LocalName(IXMLDOMNode* node) {
    BSTR text = nullptr;
    Check(node->get_baseName(&text), "invalidXml");
    _bstr_t name(text, false);
    return name.length() ? static_cast<const wchar_t*>(name) : L"";
}

/// Converts UTF-16 XML names to the UTF-8 keys used by field localization.
static std::string Utf8(const std::wstring& text) {
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

/// Decodes embedded Base64 data and reports invalid input through the application's error keys.
static std::vector<std::uint8_t> DecodeBase64(const std::wstring& encoded) {
    if (encoded.empty()) {
        return {};
    }
    DWORD size = 0;
    if (!CryptStringToBinaryW(encoded.c_str(), static_cast<DWORD>(encoded.size()), CRYPT_STRING_BASE64,
        nullptr, &size, nullptr, nullptr)) {
        throw std::runtime_error("invalidBase64");
    }
    std::vector<std::uint8_t> result(size);
    if (!CryptStringToBinaryW(encoded.c_str(), static_cast<DWORD>(encoded.size()), CRYPT_STRING_BASE64,
        result.data(), &size, nullptr, nullptr)) {
        throw std::runtime_error("invalidBase64");
    }
    result.resize(size);
    return result;
}

/// Collects XML leaf values, attributes and mixed text with their paths, separating embedded timestamp bytes
/// from display text.
static void CollectFields(IXMLDOMNode* node, const std::wstring& parentPath, std::vector<ZfoField>& fields,
    std::vector<ZfoAttachment>& binaryFields) {
    std::wstring name = LocalName(node);
    std::wstring path = parentPath + L"/" + name;
    ComPtr<IXMLDOMNamedNodeMap> attributes;
    Check(node->get_attributes(&attributes), "invalidXml");
    long count = 0;
    if (attributes) {
        Check(attributes->get_length(&count), "invalidXml");
    }
    for (long index = 0; index < count; ++index) {
        ComPtr<IXMLDOMNode> attribute;
        Check(attributes->get_item(index, &attribute), "invalidXml");
        BSTR qualified = nullptr;
        Check(attribute->get_nodeName(&qualified), "invalidXml");
        _bstr_t qualifiedName(qualified, false);
        std::wstring fullName = static_cast<const wchar_t*>(qualifiedName);
        if (fullName == L"xmlns" || fullName.starts_with(L"xmlns:") || fullName == L"xsi:nil") {
            continue;
        }
        std::wstring attributeName = LocalName(attribute.Get());
        ZfoField field{ Utf8(attributeName), Text(attribute.Get()), path + L"/@" + attributeName };
        fields.push_back(std::move(field));
    }
    if (name == L"dmFiles" || name == L"dmEncodedContent") {
        return;
    }
    ComPtr<IXMLDOMNodeList> children;
    Check(node->get_childNodes(&children), "invalidXml");
    Check(children->get_length(&count), "invalidXml");
    bool hasElements = false;
    for (long index = 0; index < count; ++index) {
        ComPtr<IXMLDOMNode> child;
        Check(children->get_item(index, &child), "invalidXml");
        DOMNodeType type{};
        Check(child->get_nodeType(&type), "invalidXml");
        if (type == NODE_ELEMENT) {
            hasElements = true;
            CollectFields(child.Get(), path, fields, binaryFields);
        }
    }
    if (hasElements) {
        for (long index = 0; index < count; ++index) {
            ComPtr<IXMLDOMNode> child;
            Check(children->get_item(index, &child), "invalidXml");
            DOMNodeType type{};
            Check(child->get_nodeType(&type), "invalidXml");
            if (type == NODE_TEXT || type == NODE_CDATA_SECTION) {
                std::wstring value = Text(child.Get());
                if (value.find_first_not_of(L" \t\r\n") != std::wstring::npos) {
                    fields.push_back(ZfoField{ Utf8(name) + "#text", std::move(value), path + L"/text()" });
                }
            }
        }
    }
    if (!hasElements) {
        ZfoField field{ Utf8(name), Text(node), path };
        if (name == L"dmQTimestamp" && !field.value.empty()) {
            ZfoAttachment binary;
            binary.name = L"timestamp-" + std::to_wstring(binaryFields.size() + 1) + L".tsr";
            binary.mimeType = L"application/timestamp-reply";
            binary.data = DecodeBase64(field.value);
            field.binary = static_cast<int>(binaryFields.size());
            field.value = std::to_wstring(binary.data.size());
            binaryFields.push_back(std::move(binary));
        }
        fields.push_back(std::move(field));
    }
}

/// Decodes a CMS data message, records its signature check and extracts XML fields and embedded attachment
/// bytes.
ZfoDocument ZfoDocument::Load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    std::streamoff length = input.tellg();
    if (!input || length <= 0 || static_cast<std::uint64_t>(length) > MAXDWORD) {
        throw std::runtime_error("invalidZfo");
    }
    ZfoDocument result;
    result.zfoBytes.resize(static_cast<size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(result.zfoBytes.data()), static_cast<std::streamsize>(length))) {
        throw std::runtime_error("invalidZfo");
    }
    CRYPT_DATA_BLOB encodedMessage{ static_cast<DWORD>(result.zfoBytes.size()), result.zfoBytes.data() };
    HCRYPTMSG rawMessage = nullptr;
    if (!CryptQueryObject(CERT_QUERY_OBJECT_BLOB, &encodedMessage, CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED,
            CERT_QUERY_FORMAT_FLAG_BINARY | CERT_QUERY_FORMAT_FLAG_BASE64_ENCODED, 0, nullptr, nullptr,
            nullptr, nullptr, &rawMessage, nullptr)) {
        throw std::runtime_error("invalidZfo");
    }
    std::unique_ptr<void, decltype(&CryptMsgClose)> message(rawMessage, CryptMsgClose);
    DWORD size = 0;
    if (!CryptMsgGetParam(message.get(), CMSG_CONTENT_PARAM, 0, nullptr, &size) || !size) {
        throw std::runtime_error("decodeFailed");
    }
    result.xmlBytes.resize(size);
    if (!CryptMsgGetParam(message.get(), CMSG_CONTENT_PARAM, 0, result.xmlBytes.data(), &size)) {
        throw std::runtime_error("decodeFailed");
    }
    result.xmlBytes.resize(size);
    DWORD signerCount = 0;
    size = sizeof(signerCount);
    result.signatureValid = CryptMsgGetParam(message.get(), CMSG_SIGNER_COUNT_PARAM, 0, &signerCount, &size)
        && signerCount > 0;
    std::wstring signers;
    for (DWORD index = 0; index < signerCount; ++index) {
        PCCERT_CONTEXT certificate = nullptr;
        DWORD signerIndex = index;
        if (!CryptMsgGetAndVerifySigner(message.get(), 0, nullptr, CMSG_USE_SIGNER_INDEX_FLAG,
                &certificate, &signerIndex)) {
            result.signatureValid = false;
            continue;
        }
        std::unique_ptr<const CERT_CONTEXT, decltype(&CertFreeCertificateContext)> signer(
            certificate, CertFreeCertificateContext);
        DWORD characters = CertGetNameStringW(certificate, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr,
            0);
        if (characters > 1) {
            std::wstring name(characters, L'\0');
            CertGetNameStringW(certificate, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name.data(),
                characters);
            name.resize(characters - 1);
            if (!signers.empty()) {
                signers += L"; ";
            }
            signers += name;
        }
    }
    ComPtr<IXMLDOMDocument2> document;
    Check(CoCreateInstance(CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&document)),
        "invalidXml");
    Check(document->put_async(VARIANT_FALSE), "Nelze nastavit XML parser.");
    Check(document->put_validateOnParse(VARIANT_FALSE), "Nelze nastavit XML parser.");
    Check(document->put_resolveExternals(VARIANT_FALSE), "invalidXml");
    Check(document->setProperty(_bstr_t(L"ProhibitDTD"), _variant_t(true)), "invalidXml");
    _variant_t source;
    source.vt = VT_ARRAY | VT_UI1;
    source.parray = SafeArrayCreateVector(VT_UI1, 0, static_cast<ULONG>(result.xmlBytes.size()));
    if (!source.parray) {
        throw std::bad_alloc();
    }
    void* bytes = nullptr;
    Check(SafeArrayAccessData(source.parray, &bytes), "invalidXml");
    memcpy(bytes, result.xmlBytes.data(), result.xmlBytes.size());
    SafeArrayUnaccessData(source.parray);
    VARIANT_BOOL loaded = VARIANT_FALSE;
    Check(document->load(source, &loaded), "invalidXml");
    if (loaded != VARIANT_TRUE) {
        throw std::runtime_error("invalidXml");
    }
    ComPtr<IXMLDOMElement> root;
    Check(document->get_documentElement(&root), "invalidXml");
    if (!root) {
        throw std::runtime_error("invalidXml");
    }
    BSTR namespaceText = nullptr;
    Check(root->get_namespaceURI(&namespaceText), "invalidXml");
    _bstr_t namespaceName(namespaceText, false);
    std::wstring space = namespaceName.length() ? static_cast<const wchar_t*>(namespaceName) : L"";
    if (!space.starts_with(L"http://isds.czechpoint.cz/v20")
            && !space.starts_with(L"http://isds.czechbox.cz/v20")) {
        throw std::runtime_error("invalidXml");
    }
    result.identifier = Value(root.Get(), L"dmID");
    result.subject = Value(root.Get(), L"dmAnnotation");
    if (result.identifier.empty()) {
        throw std::runtime_error("invalidXml");
    }
    CollectFields(root.Get(), L"", result.fields, result.binaryFields);
    bool hasType = false;
    for (const ZfoField& field : result.fields) {
        if (field.name == "dmType") {
            hasType = true;
            break;
        }
    }
    if (!hasType && LocalName(root.Get()).find(L"MessageDownloadResponse") != std::wstring::npos) {
        result.fields.push_back(ZfoField{ "dmType", L"V" });
    }
    ZfoField signature{ "signature", result.signatureValid ? L"signatureValid" : L"signatureInvalid" };
    result.fields.push_back(std::move(signature));
    if (!signers.empty()) {
        result.fields.push_back(ZfoField{ "signer", std::move(signers) });
    }
    result.fields.push_back(ZfoField{ "trust", L"trustInfo" });
    ComPtr<IXMLDOMNodeList> files;
    Check(root->selectNodes(_bstr_t(L".//*[local-name()='dmFile']"), &files), "invalidXml");
    long count = 0;
    Check(files->get_length(&count), "invalidXml");
    for (long index = 0; index < count; ++index) {
        ComPtr<IXMLDOMNode> file;
        Check(files->get_item(index, &file), "invalidXml");
        ZfoAttachment attachment;
        attachment.name = SafeFileName(Attribute(file.Get(), L"dmFileDescr"));
        attachment.mimeType = Attribute(file.Get(), L"dmMimeType");
        ComPtr<IXMLDOMNode> content = Select(file.Get(), L"./*[local-name()='dmEncodedContent']");
        if (!content || Select(content.Get(), L"./*")) {
            throw std::runtime_error("externalAttachment");
        }
        attachment.data = DecodeBase64(Text(content.Get()));
        CollectFields(file.Get(), L"", attachment.fields, result.binaryFields);
        result.attachments.push_back(std::move(attachment));
    }
    return result;
}

/// Removes directory components and replaces unsafe Windows filename characters and reserved device names.
std::wstring ZfoDocument::SafeFileName(const std::wstring& name) {
    std::wstring result = name.substr(name.find_last_of(L"/\\") + 1);
    for (wchar_t& character : result) {
        if (character < 32 || wcschr(L"<>:\"/\\|?*", character)) {
            character = L'_';
        }
    }
    while (!result.empty() && (result.back() == L'.' || result.back() == L' ')) {
        result.pop_back();
    }
    if (result.empty()) {
        result = L"attachment";
    }
    std::wstring stem = result.substr(0, result.find(L'.'));
    for (wchar_t& character : stem) {
        character = static_cast<wchar_t>(towupper(character));
    }
    const bool reservedName = stem == L"CON"
        || stem == L"PRN"
        || stem == L"AUX"
        || stem == L"NUL"
        || stem == L"CONIN$"
        || stem == L"CONOUT$";
    const bool numberedDevicePrefix = stem.size() == 4
        && (stem.starts_with(L"COM") || stem.starts_with(L"LPT"));
    const bool numberedDevice = numberedDevicePrefix
        && (stem[3] >= L'1' && stem[3] <= L'9'
            || stem[3] == L'¹'
            || stem[3] == L'²'
            || stem[3] == L'³');
    if (reservedName || numberedDevice) {
        result.insert(result.begin(), L'_');
    }
    return result;
}

/// Writes decoded bytes unchanged and reports open, write or flush failures.
void ZfoDocument::Save(const std::filesystem::path& path, const std::vector<std::uint8_t>& data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    const bool opened = static_cast<bool>(output);
    const bool written = opened
        && (data.empty() || output.write(reinterpret_cast<const char*>(data.data()), data.size()));
    const bool flushed = written && static_cast<bool>(output.flush());
    if (!flushed) {
        throw std::runtime_error("saveFailed");
    }
}

/// Formats a UTF-8 XML copy while preserving mixed content, xml:space, CDATA and explicit carriage returns.
std::vector<std::uint8_t> ZfoDocument::FormattedXml() const {
    ComPtr<IStream> input;
    ComPtr<IStream> output;
    ComPtr<IXmlReader> reader;
    ComPtr<IXmlWriter> writer;
    Check(CreateStreamOnHGlobal(nullptr, TRUE, &input), "saveFailed");
    Check(CreateStreamOnHGlobal(nullptr, TRUE, &output), "saveFailed");
    Check(input->Write(xmlBytes.data(), static_cast<ULONG>(xmlBytes.size()), nullptr), "saveFailed");
    Check(input->Seek(LARGE_INTEGER{}, STREAM_SEEK_SET, nullptr), "saveFailed");
    Check(CreateXmlReader(IID_PPV_ARGS(&reader), nullptr), "saveFailed");
    Check(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit), "saveFailed");
    Check(reader->SetInput(input.Get()), "saveFailed");
    std::vector<XmlFormatting> elements = ReadXmlFormatting(reader.Get());
    Check(input->Seek(LARGE_INTEGER{}, STREAM_SEEK_SET, nullptr), "saveFailed");
    Check(reader->SetInput(input.Get()), "saveFailed");
    Check(CreateXmlWriter(IID_PPV_ARGS(&writer), nullptr), "saveFailed");
    Check(writer->SetOutput(output.Get()), "saveFailed");
    Check(writer->SetProperty(XmlWriterProperty_Indent, FALSE), "saveFailed");
    Check(writer->SetProperty(XmlWriterProperty_ByteOrderMark, FALSE), "saveFailed");
    std::vector<size_t> parents;
    size_t element = 0;
    bool hasOutput = false;
    while (true) {
        XmlNodeType type{};
        HRESULT result = reader->Read(&type);
        if (result == S_FALSE) {
            break;
        }
        Check(result, "saveFailed");
        bool inlineParent = !parents.empty() && elements[parents.back()].inlineContent;
        if (type == XmlNodeType_Whitespace && !inlineParent) {
            continue;
        }
        bool markup = type == XmlNodeType_Element
            || type == XmlNodeType_Comment
            || type == XmlNodeType_ProcessingInstruction;
        bool blockEnd = type == XmlNodeType_EndElement && elements[parents.back()].hasMarkup;
        if (hasOutput && !inlineParent && (markup || blockEnd)) {
            size_t depth = parents.size() - (type == XmlNodeType_EndElement ? 1 : 0);
            std::wstring indentation = L"\r\n" + std::wstring(depth * 4, L' ');
            Check(writer->WriteWhitespace(indentation.c_str()), "saveFailed");
        }
        if (type == XmlNodeType_Text || type == XmlNodeType_Whitespace) {
            const wchar_t* value = nullptr;
            UINT length = 0;
            Check(reader->GetValue(&value, &length), "saveFailed");
            UINT start = 0;
            for (UINT end = 0; end <= length; ++end) {
                if (end != length && value[end] != L'\r') {
                    continue;
                }
                if (end > start) {
                    Check(writer->WriteChars(value + start, end - start), "saveFailed");
                }
                if (end < length) {
                    Check(writer->WriteCharEntity(L'\r'), "saveFailed");
                }
                start = end + 1;
            }
        } else {
            Check(writer->WriteNodeShallow(reader.Get(), FALSE), "saveFailed");
        }
        hasOutput = true;
        if (type == XmlNodeType_Element) {
            if (inlineParent) {
                elements[element].inlineContent = true;
            }
            if (!reader->IsEmptyElement()) {
                parents.push_back(element);
            }
            ++element;
        } else if (type == XmlNodeType_EndElement) {
            parents.pop_back();
        }
    }
    Check(writer->Flush(), "saveFailed");
    STATSTG information{};
    Check(output->Stat(&information, STATFLAG_NONAME), "saveFailed");
    if (information.cbSize.HighPart) {
        throw std::runtime_error("saveFailed");
    }
    std::vector<std::uint8_t> bytes(information.cbSize.LowPart);
    Check(output->Seek(LARGE_INTEGER{}, STREAM_SEEK_SET, nullptr), "saveFailed");
    ULONG copied = 0;
    Check(output->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &copied), "saveFailed");
    if (copied != bytes.size()) {
        throw std::runtime_error("saveFailed");
    }
    return bytes;
}

/// Writes the formatted XML copy to the selected export path.
void ZfoDocument::ExportXml(const std::filesystem::path& path) const {
    Save(path, FormattedXml());
}
