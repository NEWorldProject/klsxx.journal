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

#include "kls/Object.h"
#include "kls/essential/Memory.h"
#include "kls/coroutine/Async.h"
#include "kls/coroutine/Generator.h"

namespace kls::journal {
    struct AppendJournal: PmrBase {
        [[nodiscard]] virtual coroutine::ValueAsync<> append(Span<> record) = 0;
        [[nodiscard]] virtual coroutine::ValueAsync<uint64_t> register_checkpoint() = 0;
        [[nodiscard]] virtual coroutine::ValueAsync<> check_checkpoint() = 0;
        [[nodiscard]] virtual coroutine::ValueAsync<> close() = 0;
    };

    std::shared_ptr<AppendJournal> create_file_journal(std::string_view path);

    struct JournalRecord {
        int8_t type;
        Span<> data;
    };

    coroutine::AsyncGenerator<JournalRecord> recover_file_journal(std::string_view path);
}
