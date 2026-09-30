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
    PagedRom(const std::string &path, uint32_t logical_size, size_t cache_bytes);
    ~PagedRom();
    PagedRom(const PagedRom &) = delete;
    PagedRom &operator=(const PagedRom &) = delete;

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
    std::FILE *file_ = nullptr;
    std::string path_;
    uint32_t size_ = 0;
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
