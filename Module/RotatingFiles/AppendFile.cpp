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