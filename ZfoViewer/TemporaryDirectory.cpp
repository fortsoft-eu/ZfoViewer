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

#include "TemporaryDirectory.h"
#include <shlobj.h>
#include <cwchar>
#include <memory>
#include <stdexcept>

/// Scoped native handles for directory enumeration and ownership-marker access.
using FindHandle = std::unique_ptr<void, decltype(&FindClose)>;
using FileHandle = std::unique_ptr<void, decltype(&CloseHandle)>;

/// Signature and filename suffix used to recognize the application's ownership markers.
static constexpr DWORD OwnerSignature = 0x5A464F31;
static constexpr wchar_t OwnerSuffix[] = L".lock";

/// Returns the application's Local AppData cache root and optionally creates its directory tree.
static std::filesystem::path TemporaryRoot(bool create) {
    wchar_t appData[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr,
        SHGFP_TYPE_CURRENT, appData) != S_OK) {
        return {};
    }
    std::filesystem::path root = std::filesystem::path(appData) / L"FortSoft" / L"ZfoViewer";
    if (create) {
        int result = SHCreateDirectoryExW(nullptr, root.c_str(), nullptr);
        if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS) {
            return {};
        }
    }
    return root;
}

/// Recognizes only the short temporary directory names generated for this application's message tabs.
static bool IsTemporaryName(const wchar_t* name) {
    size_t length = wcslen(name);
    if (length < 8 || length > 11 || _wcsnicmp(name, L"ZFO", 3) || _wcsicmp(name + length - 4, L".tmp")) {
        return false;
    }
    for (size_t index = 3; index < length - 4; ++index) {
        wchar_t value = name[index];
        if (!(value >= L'0' && value <= L'9'
                || value >= L'A' && value <= L'F'
                || value >= L'a' && value <= L'f')) {
            return false;
        }
    }
    return true;
}

/// Attempts recursive deletion without following reparse points, leaving locked or otherwise undeletable
/// entries intact.
static bool RemoveTree(const std::filesystem::path& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return DeleteFileW(path.c_str()) != FALSE;
    }
    if (!(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        WIN32_FIND_DATAW entry{};
        std::filesystem::path pattern = path / L"*";
        HANDLE search = FindFirstFileW(pattern.c_str(), &entry);
        if (search != INVALID_HANDLE_VALUE) {
            FindHandle closer(search, FindClose);
            while (true) {
                if (wcscmp(entry.cFileName, L".") && wcscmp(entry.cFileName, L"..")) {
                    RemoveTree(path / entry.cFileName);
                }
                if (!FindNextFileW(search, &entry)) {
                    break;
                }
            }
        }
    }
    return RemoveDirectoryW(path.c_str()) != FALSE;
}

/// Removes inactive caches with a validated ownership marker, or unmarked directories only when empty.
static void RemoveStoredDirectory(const std::filesystem::path& directory) {
    if (!IsTemporaryName(directory.filename().c_str())) {
        return;
    }
    std::filesystem::path marker = directory.wstring() + OwnerSuffix;
    HANDLE file = CreateFileW(marker.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            RemoveDirectoryW(directory.c_str());
        }
        return;
    }
    FileHandle closer(file, CloseHandle);
    DWORD signature = 0;
    DWORD received = 0;
    if (!ReadFile(file, &signature, sizeof(signature), &received, nullptr)
            || received != sizeof(signature)
            || signature != OwnerSignature) {
        return;
    }
    if (RemoveTree(directory)) {
        closer.reset();
        DeleteFileW(marker.c_str());
    }
}

/// Creates a unique per-tab cache directory and holds its ownership marker open against concurrent cleanup.
TemporaryDirectory::TemporaryDirectory() {
    Cleanup();
    std::filesystem::path root = TemporaryRoot(true);
    if (root.empty()) {
        throw std::runtime_error("openFailed");
    }
    for (int attempt = 0; attempt < 64; ++attempt) {
        wchar_t candidate[MAX_PATH]{};
        if (!GetTempFileNameW(root.c_str(), L"ZFO", 0, candidate) || !DeleteFileW(candidate)) {
            throw std::runtime_error("openFailed");
        }
        if (!CreateDirectoryW(candidate, nullptr)) {
            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                continue;
            }
            throw std::runtime_error("openFailed");
        }
        directory = candidate;
        std::filesystem::path marker = directory.wstring() + OwnerSuffix;
        owner = CreateFileW(marker.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_HIDDEN, nullptr);
        if (owner == INVALID_HANDLE_VALUE) {
            owner = nullptr;
            RemoveDirectoryW(directory.c_str());
            directory.clear();
            continue;
        }
        DWORD written = 0;
        if (WriteFile(owner, &OwnerSignature, sizeof(OwnerSignature), &written, nullptr)
                && written == sizeof(OwnerSignature)) {
            return;
        }
        CloseHandle(owner);
        owner = nullptr;
        DeleteFileW(marker.c_str());
        RemoveDirectoryW(directory.c_str());
        throw std::runtime_error("openFailed");
    }
    throw std::runtime_error("openFailed");
}

/// Releases the ownership marker and attempts cleanup without propagating filesystem exceptions.
TemporaryDirectory::~TemporaryDirectory() {
    if (owner) {
        CloseHandle(owner);
    }
    try {
        RemoveStoredDirectory(directory);
    } catch (const std::exception& error) {
        OutputDebugStringA("Could not remove a closed tab's temporary directory: ");
        OutputDebugStringA(error.what());
        OutputDebugStringA("\r\n");
    }
}

/// Returns the cache directory owned by this message tab.
const std::filesystem::path& TemporaryDirectory::GetPath() const {
    return directory;
}

/// Retries removal of recognized inactive caches and their markers, skipping active owners and filesystem
/// failures.
void TemporaryDirectory::Cleanup() noexcept {
    try {
        std::filesystem::path root = TemporaryRoot(false);
        if (root.empty()) {
            return;
        }
        WIN32_FIND_DATAW entry{};
        std::filesystem::path pattern = root / L"ZFO*";
        HANDLE search = FindFirstFileW(pattern.c_str(), &entry);
        if (search == INVALID_HANDLE_VALUE) {
            return;
        }
        FindHandle closer(search, FindClose);
        while (true) {
            if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                std::filesystem::path name = entry.cFileName;
                if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && name.extension() == OwnerSuffix) {
                    name.replace_extension();
                }
                if (IsTemporaryName(name.c_str())) {
                    RemoveStoredDirectory(root / name);
                }
            }
            if (!FindNextFileW(search, &entry)) {
                break;
            }
        }
    } catch (const std::exception& error) {
        OutputDebugStringA("Could not finish deferred temporary-file cleanup: ");
        OutputDebugStringA(error.what());
        OutputDebugStringA("\r\n");
    }
}
