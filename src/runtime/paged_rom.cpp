#include "runtime/paged_rom.h"

#include <limits>
#include <stdexcept>

namespace rt {

PagedRom::PagedRom(const std::string &path, uint32_t logical_size, size_t cache_bytes, unsigned cache_ways, bool sequential_io)
    : path_(path), size_(logical_size), sequential_io_(sequential_io) {
    if (size_ < kPageBytes || (size_ & (size_ - 1)) ||
        !cache_ways || cache_bytes < kPageBytes || cache_bytes % kPageBytes || cache_bytes > size_)
        throw std::invalid_argument("paged ROM: invalid image or cache size");
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_) throw std::runtime_error("paged ROM: cannot open " + path_);
    // Must be set before any I/O. Resident data is bounded by the explicit
    // cache, not an implementation-dependent stdio read-ahead buffer.
    std::setvbuf(file_, nullptr, _IONBF, 0);
    try {
        if (std::fseek(file_, 0, SEEK_END) != 0 || std::ftell(file_) != static_cast<long>(size_) ||
            std::fseek(file_, 0, SEEK_SET) != 0)
            throw std::runtime_error("paged ROM: wrong image size or seek failure: " + path_);
        cache_.resize(cache_bytes);
        tags_.assign(cache_bytes / kPageBytes, std::numeric_limits<uint32_t>::max());
        ages_.assign(tags_.size(), 0);
        ways_ = unsigned(tags_.size() < cache_ways ? tags_.size() : cache_ways);
        while (tags_.size() % ways_) --ways_;
        sets_ = uint32_t(tags_.size() / ways_);
        set_mask_ = (sets_ & (sets_ - 1)) ? UINT32_MAX : sets_ - 1;
    } catch (...) {
        std::fclose(file_);
        file_ = nullptr;
        throw;
    }
}

PagedRom::~PagedRom() { if (file_) std::fclose(file_); }

const uint8_t *PagedRom::page(uint32_t address) const {
    const uint32_t number = (address & (size_ - 1)) / kPageBytes;
    if (last_page_ == number) {
        ++stats_.hits;
        return last_bytes_;
    }
    // Multiple ways avoid the texture-header/vertex-stream conflicts of a direct
    // mapped cache. The immediately repeated page skips the tag search, which
    // is the common case for geometry words and neighboring PCM samples.
    const uint32_t set = set_mask_ == UINT32_MAX ? number % sets_ : number & set_mask_;
    const size_t first = size_t(set) * ways_;
    size_t slot = first;
    ++serial_;
    for (unsigned way = 0; way < ways_; ++way) {
        const size_t candidate = first + way;
        if (tags_[candidate] == number) {
            ages_[candidate] = serial_;
            last_page_ = number;
            last_bytes_ = cache_.data() + candidate * kPageBytes;
            ++stats_.hits;
            return last_bytes_;
        }
        if (tags_[candidate] == UINT32_MAX ||
            (tags_[slot] != UINT32_MAX && uint32_t(serial_ - ages_[candidate]) > uint32_t(serial_ - ages_[slot])))
            slot = candidate;
    }
    uint8_t *const bytes = cache_.data() + slot * kPageBytes;
    // Invalidate before I/O: a short read must never leave a previous tag
    // attached to a partially overwritten slot, including the last-page path.
    tags_[slot] = std::numeric_limits<uint32_t>::max();
    last_page_ = UINT32_MAX;
    last_bytes_ = nullptr;
    const uint32_t offset = number * kPageBytes;
    const bool need_seek = !sequential_io_ || next_read_ != offset;
    next_read_ = UINT32_MAX; // Failed/short I/O makes position unknown.
    if (need_seek) {
        observe(IoEvent::SeekBegin);
        const int seek_result = std::fseek(file_, static_cast<long>(offset), SEEK_SET);
        observe(seek_result ? IoEvent::SeekFailed : IoEvent::SeekEnd);
        if (seek_result) throw std::runtime_error("paged ROM: read failure: " + path_);
    }
    observe(IoEvent::ReadBegin);
    const size_t read_result = std::fread(bytes, 1, kPageBytes, file_);
    observe(read_result == kPageBytes ? IoEvent::ReadEnd : IoEvent::ReadFailed);
    if (read_result != kPageBytes)
        throw std::runtime_error("paged ROM: read failure: " + path_);
    next_read_ = offset + kPageBytes;
    tags_[slot] = number;
    ages_[slot] = serial_;
    last_page_ = number;
    last_bytes_ = bytes;
    ++stats_.misses;
    stats_.bytes_read += kPageBytes;
    return bytes;
}

uint8_t PagedRom::read8(uint32_t address) const {
    return page(address)[address & (kPageBytes - 1)];
}

uint16_t PagedRom::read16(uint32_t address) const {
    const uint32_t offset = address & (kPageBytes - 1);
    if (offset == kPageBytes - 1)
        return uint16_t(uint16_t(read8(address)) | (uint16_t(read8(address + 1)) << 8));
    const uint8_t *const p = page(address) + offset;
    return uint16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
}

uint32_t PagedRom::read32(uint32_t address) const {
    const uint32_t offset = address & (kPageBytes - 1);
    if (offset > kPageBytes - 4) {
        uint32_t value = 0;
        for (unsigned lane = 0; lane < 4; ++lane) value |= uint32_t(read8(address + lane)) << (lane * 8);
        return value;
    }
    const uint8_t *const p = page(address) + offset;
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

} // namespace rt
