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

#include "Common.h"
#include "ActiveFile.h"

namespace kls::journal::rotating_file::detail {
    AppendFile::AppendFile(fs::path &base, std::uint64_t id) :
            m_base(base), m_id(id), m_active(std::make_shared<ActiveFile>(base, id)) {}

    std::optional<coroutine::FlexFuture<>> AppendFile::append(int8_t type, essential::Span<> record) {
        if (m_state == S_ACTIVE) {
            const auto active = static_cast<ActiveFile *>(m_active.get());
            return active->append(type, record);
        }
        return std::nullopt;
    }

    coroutine::ValueAsync<> AppendFile::close() {
        if (m_state != S_ACTIVE) return []() -> coroutine::ValueAsync<> { co_return; }(); else m_state = S_STUB;
        auto handle = std::static_pointer_cast<ActiveFile>(std::move(m_active));
        auto fn_close = [](std::shared_ptr<ActiveFile> handle) -> coroutine::ValueAsync<> { co_await handle->close(); };
        return fn_close(std::move(handle));
    }

    void AppendFile::remove() {
        if (m_state != S_STUB) throw std::logic_error("invalid state"); else m_state = S_REMOVED;
        fs::remove(m_base / std::format("{}.{}", m_id, FileExtension));
    }
}