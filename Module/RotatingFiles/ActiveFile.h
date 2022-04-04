#pragma once

#include <optional>
#include "Common.h"
#include "kls/io/Block.h"

namespace kls::journal::rotating_file::detail {
    using LazyFile = coroutine::LazyAsync<std::unique_ptr<io::Block>>;

    class ActiveFile : public AddressSensitive {
    public:
        explicit ActiveFile(const fs::path &base, std::uint64_t id);
        std::optional<coroutine::FlexFuture<>> append(int8_t type, essential::Span<> record);
        coroutine::ValueAsync<> close();
    private:
        LazyFile m_file;
        essential::Span<> m_buffer;
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