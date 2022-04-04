#include "ActiveFile.h"
#include "kls/thread/SpinWait.h"
#include "kls/essential/Unsafe.h"

namespace kls::journal::rotating_file::detail {
    static constexpr auto FileOption = io::Block::F_CREAT | io::Block::F_WRITE;

    static LazyFile open_file(const fs::path &base, uint64_t id) {
        auto path = base / std::format("{}.{}", id, FileExtension);
        auto path_string = path.generic_string();
        auto handle = co_await io::open_block(path_string, FileOption);
        co_return std::move(handle);
    }

    ActiveFile::ActiveFile(const fs::path &base, uint64_t id) :
            m_file(open_file(base, id)),
            m_buffer(reinterpret_cast<void *>(essential::rent_4m_block()), MaxFileSize) {
    }

    coroutine::ValueAsync<> ActiveFile::close() {
        if (m_last_writer) co_await std::move(m_last_writer);
        essential::return_4m_block(reinterpret_cast<uintptr_t>(m_buffer.data()));
        co_await ((co_await m_file)->close());
    }

    std::optional<coroutine::FlexFuture<>> ActiveFile::append(int8_t type, essential::Span<> record) {
        // try to allocate space for the commit operation, return nullopt on failure
        // since we need the allocation counter to correctly close the file, we need to do CAS
        auto record_view = essential::static_span_cast<char>(record);
        int32_t allocation{}, end_offset{};
        for (;;) {
            allocation = m_allocation_offset.load();
            end_offset = allocation + int32_t(record.size()) + 4;
            if (end_offset > MaxFileSize) return std::nullopt;
            if (m_allocation_offset.compare_exchange_weak(allocation, end_offset)) break;
        }
        // trim the buffer to get the allocated segment as a span
        auto buffer_view = essential::static_span_cast<char>(record).trim_front(allocation);
        // write the message header
        const auto header = uint32_t(record_view.size()) << uint32_t(8) | uint32_t(type);
        essential::Access<endian>(buffer_view).put<uint32_t>(0, header);
        // allocation is set, copy to buffer does not require synchronization
        std::ranges::copy(record_view, buffer_view.trim_front(4).begin());
        // sequencing for the commit operation
        // as 1MiB max memcpy should not take much time, we can simply spin-wait
        thread::SpinWait wait{};
        for (;;) {
            auto commit_offset = m_commit_offset.load();
            if (commit_offset == allocation) {
                m_commit_offset.store(end_offset);
                break;
            }
            wait.once();
        }
        // batched update
        std::lock_guard lk{m_sequence};
        // update the batch offset so the writer knows there is more work
        // as this lock can be taken out of order, we need to sequence again
        // however, since anything before end_offset is guaranteed to have finished copying by the last sequencing,
        // we can simply only update the batch writer's offset if the end_offset is larger
        if (m_batch_offset < end_offset) m_batch_offset = end_offset;
        // observer based on the stage
        switch (m_batch_writer_stage) {
            case BS_NONE:
                batch_writer();
                [[fallthrough]];
            case BS_LIVE:
                std::construct_at(&m_future.value, [this](auto h) noexcept { m_promise = std::move(h); });
                m_batch_writer_stage = BS_PENDING;
                [[fallthrough]];
            default:
                break; // for BS_PENDING nothing need to be done
        }
        return std::optional{m_future.value};
    }

    void ActiveFile::batch_writer() { m_last_writer = batch_writer_work(std::move(m_last_writer)); }

    coroutine::ValueAsync<> ActiveFile::batch_writer_work(coroutine::ValueAsync<> last) {
        auto &file = co_await m_file;
        std::unique_lock lk{m_sequence};
        for (;;) {
            auto promise = batch_writer_live();
            int32_t start_offset = m_file_offset, end_offset = m_batch_offset;
            lk.unlock();
            try {
                auto batched_span = m_buffer.keep_front(end_offset).trim_front(start_offset);
                (co_await file->write(batched_span, start_offset)).get_result(), promise->set();
            }
            catch (...) { promise->fail(std::current_exception()); }
            lk.lock();
            if (m_batch_writer_stage != BS_PENDING) co_return void(m_batch_writer_stage = BS_NONE);
        }
        if (last) co_await std::move(last); // consume the future for the last writer to minimize blocking
    }

    coroutine::FlexFuture<>::PromiseHandle ActiveFile::batch_writer_live() {
        // set stage to LIVE if it has not been
        if (m_batch_writer_stage != BS_LIVE) m_batch_writer_stage = BS_LIVE;
        // destroy the current shared future and get the current promise
        std::destroy_at(&m_future.value);
        return std::move(m_promise);
    }
}