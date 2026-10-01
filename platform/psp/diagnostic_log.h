#pragma once

#include <pspiofilemgr.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace psp {

// Capture a checked getcwd() on main and initialize before any worker starts.
// Paths then stay immutable. Exactly one caller may write at a time; error and
// record counters can be read concurrently. Native I/O deliberately bypasses
// newlib's descriptor table and FILE locks. Native workers do not inherit
// newlib's global working-directory expansion.
class DiagnosticLog {
public:
    static constexpr size_t kPathCapacity = 1024;
    static constexpr size_t kDeviceCapacity = 32;
    static constexpr size_t kFileLimit = 2 * 1024 * 1024;
    // Local validation failures; native failures retain their original code.
    static constexpr int32_t kInvalidPath = -1, kPathTooLong = -2;
    static constexpr int32_t kNotInitialized = -3, kInvalidRecord = -4;
    static constexpr int32_t kFileFull = -5, kNoWriteProgress = -6;
    static constexpr int32_t kInvalidWriteCount = -7;

    bool initialize(const char* absolute_cwd) noexcept {
        if (ready_ || !absolute_cwd) return fail(kInvalidPath);
        size_t length = 0;
        while (length < kPathCapacity && absolute_cwd[length]) ++length;
        if (length == kPathCapacity) return fail(kPathTooLong);

        size_t colon = 0;
        while (colon < length && absolute_cwd[colon] != ':') {
            const char c = absolute_cwd[colon];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9'))) return fail(kInvalidPath);
            ++colon;
        }
        if (!colon || colon >= length || colon + 1 >= length ||
            absolute_cwd[colon + 1] != '/') return fail(kInvalidPath);
        if (colon + 1 >= kDeviceCapacity) return fail(kPathTooLong);
        for (size_t i = colon + 2; i < length; ++i) {
            const unsigned char c = static_cast<unsigned char>(absolute_cwd[i]);
            if (c < 32 || c == 127 || c == ':' || c == '\\') return fail(kInvalidPath);
        }
        // getcwd supplies an absolute normalized path. Reject traversal rather
        // than letting a malformed caller move diagnostics outside that path.
        for (size_t start = colon + 2; start < length;) {
            size_t end = start;
            while (end < length && absolute_cwd[end] != '/') ++end;
            if ((end - start == 1 && absolute_cwd[start] == '.') ||
                (end - start == 2 && absolute_cwd[start] == '.' && absolute_cwd[start + 1] == '.'))
                return fail(kInvalidPath);
            start = end + 1;
        }
        while (length > colon + 2 && absolute_cwd[length - 1] == '/') --length;
        const bool slash = absolute_cwd[length - 1] != '/';
        constexpr char diagnostic_name[] = "psp-diagnostic.log";
        constexpr char stall_name[] = "psp-stall.log";
        if (length + size_t(slash) + sizeof(diagnostic_name) > kPathCapacity)
            return fail(kPathTooLong);

        std::memcpy(device_, absolute_cwd, colon + 1);
        device_[colon + 1] = '\0';
        for (unsigned i = 0; i < 2; ++i) {
            std::memcpy(paths_[i], absolute_cwd, length);
            size_t offset = length;
            if (slash) paths_[i][offset++] = '/';
            const char* name = i ? stall_name : diagnostic_name;
            std::memcpy(paths_[i] + offset, name, i ? sizeof(stall_name) : sizeof(diagnostic_name));
        }
        ready_ = true;
        return true;
    }

    bool write(bool stall_file, const char* text, size_t length, bool sync = true) noexcept {
        if (!ready_) return fail(kNotInitialized);
        if (!text || !length) return fail(kInvalidRecord);
        if (length > kFileLimit) return fail(kFileFull);
        const SceUID file = sceIoOpen(path(stall_file), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
        if (file < 0) return fail(int32_t(file));

        bool complete = true;
        const int end = sceIoLseek32(file, 0, PSP_SEEK_END);
        if (end < 0) { fail(int32_t(end)); complete = false; }
        else if (size_t(end) > kFileLimit || length > kFileLimit - size_t(end)) {
            fail(kFileFull); complete = false;
        }
        size_t offset = 0;
        while (complete && offset < length) {
            const size_t remaining = length - offset;
            const int wrote = sceIoWrite(file, text + offset, static_cast<SceSize>(remaining));
            // sceIoWrite reports native errors, not libc errno/EINTR. Do not
            // replay a failed write without an established kernel contract.
            if (wrote < 0) { fail(int32_t(wrote)); complete = false; }
            else if (!wrote) { fail(kNoWriteProgress); complete = false; }
            else if (size_t(wrote) > remaining) { fail(kInvalidWriteCount); complete = false; }
            else offset += size_t(wrote);
        }
        const int closed = sceIoClose(file);
        if (closed < 0) { fail(int32_t(closed)); complete = false; }
        // Also try to preserve a partial record after an I/O failure. Closing
        // and syncing do not make a physical power loss atomic or recoverable.
        if (sync && offset) {
            const int synced = sceIoSync(device_, 0);
            if (synced < 0) { fail(int32_t(synced)); complete = false; }
        }
        if (complete) records_.fetch_add(1, std::memory_order_relaxed);
        return complete;
    }

    bool ready() const noexcept { return ready_; }
    const char* path(bool stall_file) const noexcept { return paths_[stall_file ? 1 : 0]; }
    int32_t error() const noexcept { return error_.load(std::memory_order_relaxed); }
    uint32_t records() const noexcept { return records_.load(std::memory_order_relaxed); }

private:
    static_assert(std::atomic<int32_t>::is_always_lock_free && std::atomic<uint32_t>::is_always_lock_free);
    char device_[kDeviceCapacity]{};
    char paths_[2][kPathCapacity]{};
    bool ready_ = false;
    std::atomic<int32_t> error_{0};
    std::atomic<uint32_t> records_{0};
    bool fail(int32_t code) noexcept {
        int32_t expected = 0;
        error_.compare_exchange_strong(expected, code, std::memory_order_relaxed);
        return false;
    }
};

} // namespace psp
