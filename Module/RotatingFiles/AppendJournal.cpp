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
#include "kls/essential/Unsafe.h"
#include "kls/coroutine/Operation.h"

static constexpr auto err_non_empty = "given path for appending journal is not empty: {}";

namespace kls::journal::rotating_file::detail {
    struct CheckRecord {
        char buffer[16]{};
        CheckRecord(uint64_t last, uint64_t now) noexcept {
            essential::Access<endian> access{{buffer, 16}};
            access.put(0, last);
            access.put(8, now);
        }
        essential::Span<> span() & noexcept { return {buffer, 16}; }
    };

    AppendJournal::AppendJournal(const fs::path &base) : m_base(prepare_path(base)) {
        if (auto&&[a, b] = scan_files(base); a || b)
            throw std::runtime_error(std::format(err_non_empty, base.generic_string()));
    }

    coroutine::ValueAsync<> AppendJournal::append(essential::Span<> record) {
        if (record.size() + 4 > MaxRecordSize) throw std::runtime_error("journal record size too large");
        m_lock.lock();
        return append_internal(RTypeData, record);
    }

    coroutine::ValueAsync<> AppendJournal::append_internal(int8_t type, essential::Span<> record) {
        std::unique_lock lk{m_lock, std::adopt_lock};
        m_segment_empty = false;
        coroutine::ValueAsync<> to_close{};
        for (;;) {
            if (m_files.empty()) break;
            auto file = &m_files.back();
            if (auto opt = (lk.unlock(), file->append(type, record)); opt) return coroutine::awaits(*std::move(opt));
            lk.lock();
            if (file->id() == m_files.back().id()) {
                to_close = file->close();
                break;
            }
        }
        m_files.emplace_back(m_base, m_next_file++);
        auto &file = m_files.back();
        auto hint = CheckRecord(get_last_checkpoint(), get_current_checkpoint());
        lk.unlock();
        auto commit_hint = *file.append(RTypeCheck, hint.span());
        auto commit_record = *file.append(type, record);
        if (to_close)
            return coroutine::awaits(std::move(to_close), std::move(commit_hint), std::move(commit_record));
        else
            return coroutine::awaits(std::move(commit_hint), std::move(commit_record));
    }

    coroutine::ValueAsync<uint64_t> AppendJournal::register_checkpoint() {
        std::unique_lock lk{m_lock};
        if (m_segment_empty) co_return get_current_checkpoint(); else m_segment_empty = true;
        m_checkpoints[m_next_checkpoint++] = m_files.back().id();
        auto current_checkpoint = get_current_checkpoint();
        auto record = CheckRecord(get_last_checkpoint(), current_checkpoint);
        co_await (lk.release(), append_internal(RTypeCheck, record.span()));
        co_return current_checkpoint;
    }

    coroutine::ValueAsync<> AppendJournal::check_checkpoint() {
        std::unique_lock lk{m_lock};
        auto last_keep_id = m_checkpoints.begin()->second;
        while (!m_files.empty()) {
            auto &file = m_files.front();
            if (file.id() >= last_keep_id) break;
            // as there is only one "active" file, there is no worry to remove a not-closed file
            m_files.front().remove();
            m_files.erase(m_files.begin());
        }
        m_checkpoints.erase(m_checkpoints.begin());
        auto record = CheckRecord(get_last_checkpoint(), get_current_checkpoint());
        co_await (lk.release(), append_internal(RTypeCheck, record.span()));
    }

    coroutine::ValueAsync<> AppendJournal::close() {
        // there is no need to clear the files, as it could be a graceful shutdown due to a failed dependent service
        // we just close every single file in the chain, without synchronization
        std::vector<coroutine::ValueAsync<>> ops{};
        ops.reserve(m_files.size());
        for (auto&& x : m_files) ops.push_back(x.close());
        co_await coroutine::await_all(std::move(ops));
    }
}

namespace kls::journal {
    std::shared_ptr<AppendJournal> create_file_journal(std::string_view path) {
        return std::make_shared<rotating_file::detail::AppendJournal>(std::filesystem::path(path));
    }
}