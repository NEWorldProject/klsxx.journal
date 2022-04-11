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

#include <charconv>
#include <filesystem>
#include <gtest/gtest.h>
#include "kls/Format.h"
#include "kls/journal/Journal.h"
#include "kls/coroutine/Blocking.h"
#include "kls/coroutine/Operation.h"

TEST(kls_journal, JournalEcho) {
    using namespace kls;
    using namespace kls::journal;
    using namespace kls::coroutine;

    auto to_hex = [](Span<char> span) -> std::string {
        std::string result(size_t(span.size() * 3), ' ');
        char *ptr = result.data();
        for (char x: span) {
            auto[p, ec] = std::to_chars(ptr, ptr + 3, x, 16);
            ptr = p;
        }
        return result;
    };

    static constexpr auto payload1 = std::string_view("Hello World");
    static constexpr auto payload2 = std::string_view("The red fox jumped over the lazy brown dog");

    auto Write = []() -> ValueAsync<bool> {
        auto append = create_file_journal("./test.kls.journal.file");
        co_await uses(append, [](AppendJournal &file) -> ValueAsync<> {
            co_await awaits(file.append(Span<char>{payload1}), file.append(Span<char>{payload2}));
            co_await file.register_checkpoint();
            co_await awaits(file.append(Span<char>{payload1}), file.append(Span<char>{payload2}));
            co_await file.check_checkpoint();
        });
        co_return true;
    };

    auto Read = [&]() -> ValueAsync<bool> {
        auto recover = recover_file_journal("./test.kls.journal.file");
        while (co_await recover.forward()) {
            JournalRecord record = recover.next();
            std::string formatted{};
            auto range = static_span_cast<char>(record.data);
            if (record.type == 0)
                formatted = kls::format("[0:{}]:{}", range.size(), std::string_view(range.begin(), range.end()));
            else
                formatted = kls::format("[{}:{}]:{}", record.type, range.size(), to_hex(range));
            std::cout << formatted << std::endl;
        }
        co_return true;
    };

    auto success = run_blocking([&]() -> ValueAsync<bool> {
        try {
            auto write = co_await Write();
            auto read = co_await Read();
            std::filesystem::remove_all("./test.kls.journal.file");
            co_return write && read;
        }
        catch (...) {
            std::filesystem::remove_all("./test.kls.journal.file");
            throw;
        }
    });
    ASSERT_TRUE(success);
}