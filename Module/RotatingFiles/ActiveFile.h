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

#pragma once

#include <optional>
#include "Common.h"
#include "kls/io/Block.h"

namespace kls::journal::rotating_file::detail {
    using LazyFile = coroutine::LazyAsync<SafeHandle<io::Block>>;

    class ActiveFile : public AddressSensitive {
    public:
        explicit ActiveFile(const fs::path &base, std::uint64_t id);
        std::optional<coroutine::FlexFuture<>> append(int8_t type, Span<> record);
        coroutine::ValueAsync<> close();
    private:
        LazyFile m_file;
        Buffer m_buffer;
        // insert operation helper
        std::atomic_int32_t m_allocation_offset{0}, m_commit_offset{0};
        // batch writer states
        enum BatchStage {
            BS_NONE, BS_PENDING, BS_LIVE
        };
        thread::SpinLock m_sequence{}; // this lock protect everything below
        BatchStage m_batch_writer_stage{BS_NONE};
        int32_t m_batch_offset{0}, m_file_offset{0};
        Storage<coroutine::FlexFuture<>> m_future{};
        coroutine::FlexFuture<>::PromiseHandle m_promise{};

        // The batch writer and its future
        void batch_writer();
        coroutine::ValueAsync<> m_last_writer{};
        coroutine::FlexFuture<>::PromiseHandle batch_writer_live();
        coroutine::ValueAsync<> batch_writer_work(coroutine::ValueAsync<> last);
    };
}