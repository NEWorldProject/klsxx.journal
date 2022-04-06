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

#include <format>
#include <vector>
#include "Common.h"
#include "kls/io/Block.h"
#include "kls/essential/Unsafe.h"
#include "kls/coroutine/Operation.h"

namespace kls::journal::rotating_file::detail {
    using namespace kls::coroutine;

    static constexpr auto FileOption = io::Block::F_READ;

    static auto open_with_id(const fs::path &base, uint64_t id) {
        auto path = base / std::format("{}{}", id, FileExtension);
        auto path_string = path.generic_string();
        return io::open_block(path_string, FileOption);
    }
}

namespace kls::journal {
    using namespace kls::coroutine;
    using namespace kls::journal::rotating_file::detail;

    coroutine::AsyncGenerator<JournalRecord> recover_file_journal(std::string_view path) {
        auto root = prepare_path(fs::absolute(path));
        auto[a, b] = scan_files(root);
        for (uint64_t id = a; id <= b; ++id) {
            Buffer buffer{};
            auto file = co_await open_with_id(root, id);
            auto file_size = co_await uses(file, [&buffer](io::Block &file) -> ValueAsync<int> {
                co_return (co_await file.read(buffer.span(), 0)).get_result();
            });
            auto file_span = buffer.span().keep_front(file_size);
            auto file_reader = essential::SpanReader<endian>(file_span);
            while (file_reader.check<uint32_t>(1)) {
                const auto header = file_reader.get<uint32_t>();
                const auto type = int8_t(header & 0xFF);
                const auto size = int(header >> 8u);
                if (!file_reader.check<char>(size)) throw std::runtime_error("bad journal");
                co_yield JournalRecord{.type = type, .data=file_reader.bytes(size)};
            }
        }
    }
}