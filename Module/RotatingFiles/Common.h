#pragma once

#include <bit>
#include <map>
#include <list>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include "kls/thread/SpinLock.h"
#include "kls/journal/Journal.h"
#include "kls/coroutine/Future.h"

namespace kls::journal::rotating_file::detail {
    namespace fs = std::filesystem;

    // constants for rotating file
    static constexpr auto endian = std::endian::little;
    static constexpr int32_t MaxFileSize = 4 << 20;
    static constexpr int32_t MaxRecordSize = 1 << 20;
    static constexpr std::string_view FileExtension = ".journal";

    static constexpr int8_t RTypeData = 0;
    static constexpr int8_t RTypeCheck = 1;

    // File Main Interface
    class AppendFile {
    public:
        enum State {
            S_ACTIVE, // file active for append
            S_STUB, // file not open, can be only removed
            S_REMOVED // the file is removed
        };
        explicit AppendFile(fs::path &base, std::uint64_t id);
        [[nodiscard]] uint64_t id() const noexcept { return m_id; }
        [[nodiscard]] std::optional<coroutine::FlexFuture<>> append(int8_t type, essential::Span<> record);
        [[nodiscard]] coroutine::ValueAsync<> close();
        void remove();
    private:
        fs::path &m_base;
        std::uint64_t m_id;
        State m_state{S_ACTIVE};
        std::shared_ptr<void> m_active;
    };

    class AppendJournal: public kls::journal::AppendJournal {
    public:
        explicit AppendJournal(const fs::path &base);
        [[nodiscard]] coroutine::ValueAsync<> append(essential::Span<> record) override;
        [[nodiscard]] coroutine::ValueAsync<uint64_t> register_checkpoint() override;
        [[nodiscard]] coroutine::ValueAsync<> check_checkpoint() override;
        [[nodiscard]] coroutine::ValueAsync<> close() override;
    private:
        fs::path m_base;
        thread::SpinLock m_lock;
        bool m_segment_empty{false};
        std::list<AppendFile> m_files;
        std::map<uint64_t, uint64_t> m_checkpoints;
        uint64_t m_next_file{0}, m_next_checkpoint{0};

        uint64_t get_last_checkpoint() const noexcept {
            if (m_checkpoints.empty()) return 0; else return m_checkpoints.begin()->first;
        }
        uint64_t get_current_checkpoint() const noexcept { return m_next_checkpoint; }
        coroutine::ValueAsync<> append_internal(int8_t type, essential::Span<> record);
    };

    fs::path prepare_path(const fs::path &path);
    std::pair<uint64_t, uint64_t> scan_files(const fs::path &root);
}