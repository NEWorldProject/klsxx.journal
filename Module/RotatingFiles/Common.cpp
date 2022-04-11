/*
* Copyright (c) 2022 DWVoid and Infinideastudio Team
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

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <set>
#include <charconv>
#include "Common.h"
#include "kls/Format.h"

static constexpr auto err_not_a_directory = "given path for journal is not a directory: {}";
static constexpr auto err_duplicate_id = "journal with id {} in directory {} has a duplicate";
static constexpr auto err_missing_id = "journal in directory {} has a missing journal id in range [{}, {}]";

namespace kls::journal::rotating_file::detail {
    fs::path prepare_path(const fs::path& path) {
        auto absolute = fs::absolute(path);
        if (fs::exists(absolute)) {
            if (fs::is_directory(absolute)) return absolute;
            throw std::runtime_error(kls::format(err_not_a_directory, absolute.generic_string()));
        }
        return fs::create_directories(absolute), absolute;
    }

    std::pair<uint64_t, uint64_t> scan_files(const fs::path &root) {
        std::set<uint64_t> files{};
        for (auto &&entry: fs::directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            const auto path = entry.path();
            if (path.extension() != FileExtension) continue;
            uint64_t result{};
            const auto stem = path.stem().generic_string();
            const auto end_ptr = stem.data() + stem.size();
            auto [ptr, ec] { std::from_chars(stem.data(), end_ptr, result) };
            if (ec == std::errc() && ptr == end_ptr) {
                if (files.contains(result))
                    throw std::runtime_error(kls::format(err_duplicate_id, result, root.generic_string()));
                files.insert(result);
            }
        }
        if (files.empty()) return {0, 0};
        auto result = std::pair(*files.begin(), *files.rbegin());
        if ((result.second - result.first) != (files.size() - 1))
            throw std::runtime_error(kls::format(err_missing_id, root.generic_string(), result.first, result.second));
        return result;
    }
}