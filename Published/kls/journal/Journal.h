#pragma once

#include "kls/Object.h"
#include "kls/essential/Memory.h"
#include "kls/coroutine/Async.h"
#include "kls/coroutine/Generator.h"

namespace kls::journal {
    struct AppendJournal: PmrBase {
        [[nodiscard]] virtual coroutine::ValueAsync<> append(essential::Span<> record) = 0;
        [[nodiscard]] virtual coroutine::ValueAsync<uint64_t> register_checkpoint() = 0;
        [[nodiscard]] virtual coroutine::ValueAsync<> check_checkpoint() = 0;
        [[nodiscard]] virtual coroutine::ValueAsync<> close() = 0;
    };

    std::shared_ptr<AppendJournal> create_file_journal(std::string_view path);
}
