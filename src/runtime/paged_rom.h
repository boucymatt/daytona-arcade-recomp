// Bounded, read-only storage for large imported ROM images. The logical image
// stays its original power-of-two size; only cache pages occupy host RAM.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace rt {

class PagedRom {
public:
    static constexpr uint32_t kPageBytes = 4096;
    struct Stats { uint64_t hits = 0, misses = 0, bytes_read = 0; };
    // Optional cache policy; defaults preserve existing platform behavior.
    // Sequential I/O skips a seek only after a verified full read ending at
    // the exact next requested offset. This object exclusively owns its FILE.
    PagedRom(const std::string &path, uint32_t logical_size, size_t cache_bytes, unsigned cache_ways = 4, bool sequential_io = false);
    ~PagedRom();
    PagedRom(const PagedRom &) = delete;
    PagedRom &operator=(const PagedRom &) = delete;

    enum class IoEvent { SeekBegin, SeekEnd, SeekFailed, ReadBegin, ReadEnd, ReadFailed };
    using IoObserver = void (*)(void*, IoEvent) noexcept;
    // Configure on owner before use; observer/context must outlive this image.
    // No callbacks on cache hits. Default is null on every platform.
    void set_io_observer(IoObserver observer, void* context) { observer_ = observer; observer_context_ = context; }
    uint32_t size() const { return size_; }
    size_t cache_bytes() const { return cache_.size(); }
    const Stats &stats() const { return stats_; }
    // Little-endian, including unaligned reads crossing a page or image end.
    // Calls belong to one owner thread. Separate main/audio images have their
    // own files and caches; concurrent access to one image is not supported.
    // I/O failures throw, never fabricate ROM data or return stale cache data.
    uint8_t read8(uint32_t address) const;
    uint16_t read16(uint32_t address) const;
    uint32_t read32(uint32_t address) const;

private:
    IoObserver observer_ = nullptr;
    void* observer_context_ = nullptr;
    void observe(IoEvent event) const noexcept { if (observer_) observer_(observer_context_, event); }
    std::FILE *file_ = nullptr;
    std::string path_;
    uint32_t size_ = 0;
    bool sequential_io_ = false;
    mutable uint32_t next_read_ = UINT32_MAX;
    mutable std::vector<uint8_t> cache_;
    mutable std::vector<uint32_t> tags_, ages_;
    uint32_t sets_ = 1, set_mask_ = 0;
    unsigned ways_ = 1;
    mutable uint32_t serial_ = 0, last_page_ = UINT32_MAX;
    mutable const uint8_t *last_bytes_ = nullptr;
    mutable Stats stats_;
    const uint8_t *page(uint32_t address) const;
};

} // namespace rt
