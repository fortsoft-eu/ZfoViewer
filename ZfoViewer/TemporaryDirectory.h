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
#include <filesystem>

/// Owns a unique Local AppData attachment cache and its exclusive cleanup marker for one message tab.
class TemporaryDirectory {

public:
    /// Creates a unique per-tab cache directory and holds its ownership marker open against concurrent
    /// cleanup.
    TemporaryDirectory();

    /// Releases the ownership marker and attempts cleanup without propagating filesystem exceptions.
    ~TemporaryDirectory();

    /// Prevents two objects from owning and cleaning up the same cache directory.
    TemporaryDirectory(const TemporaryDirectory&) = delete;

    /// Prevents reassignment of the directory and its exclusive ownership marker.
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    /// Returns the cache directory owned by this message tab.
    const std::filesystem::path& GetPath() const;

    /// Retries removal of recognized inactive caches and their markers, skipping active owners and filesystem
    /// failures.
    static void Cleanup() noexcept;

private:
    /// Exclusive ownership-marker handle and the temporary directory protected by it.
    HANDLE owner = nullptr;
    std::filesystem::path directory;
};
