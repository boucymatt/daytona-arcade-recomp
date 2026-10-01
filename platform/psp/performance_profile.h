// Diagnostic publication only. Counters never control game/audio execution.
#pragma once
#include "runtime/paged_rom.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace psp {
struct IoCounters {
    std::atomic<uint32_t> phase{0}, began{0}, seeks{0}, reads{0}, failures{0};
    std::atomic<uint32_t> seek_us{0}, read_us{0};
    uint32_t owner_begin = 0; // Only the region's owning thread touches this.
    void event(rt::PagedRom::IoEvent event, uint32_t now) {
        using E = rt::PagedRom::IoEvent;
        if (event == E::SeekBegin || event == E::ReadBegin) {
            owner_begin = now;
            began.store(now); phase.store(event == E::SeekBegin ? 1 : 2);
        } else {
            const bool seek = event == E::SeekEnd || event == E::SeekFailed;
            (seek ? seek_us : read_us).fetch_add(now - owner_begin);
            if (event == E::SeekFailed || event == E::ReadFailed) failures.fetch_add(1);
            else (seek ? seeks : reads).fetch_add(1);
            phase.store(0);
        }
    }
};
class FrameTimings {
public:
    enum Field { Frame, Board, Core, Geometry, Video, Raster, TileCache, TileDraw,
                 Composite, Present, RomSeek, RomRead, RomPages, Count };
    using Snapshot = std::array<uint32_t, Count>;
    void publish(const Snapshot& values) {
        sequence_.fetch_add(1);
        for (unsigned i = 0; i < Count; ++i) fields_[i].store(values[i]);
        sequence_.fetch_add(1);
    }
    bool read(Snapshot& values) const {
        const auto before = sequence_.load();
        if (!before || (before & 1)) return false;
        for (unsigned i = 0; i < Count; ++i) values[i] = fields_[i].load();
        return sequence_.load() == before;
    }
private:
    // Sequential consistency makes a successful bounded read coherent. Never
    // spin: an observer may have preempted the lower-priority writer mid-copy.
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    std::atomic<uint32_t> sequence_{0};
    std::array<std::atomic<uint32_t>, Count> fields_{};
};
} // namespace psp
