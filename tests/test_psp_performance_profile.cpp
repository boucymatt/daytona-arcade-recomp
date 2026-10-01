#include "performance_profile.h"
#include <cstdio>
#include <cstdlib>
#include <thread>

void require(bool value) { if (!value) std::abort(); }
int main() {
    using E = rt::PagedRom::IoEvent;
    psp::IoCounters io;
    io.event(E::SeekBegin, 0xfffffff0u);
    require(io.phase == 1);
    io.event(E::SeekEnd, 0x10u);
    require(io.seek_us == 32 && io.seeks == 1 && io.phase == 0);
    io.event(E::ReadBegin, 100);
    io.event(E::ReadEnd, 175);
    require(io.read_us == 75 && io.reads == 1);
    io.event(E::SeekBegin, 200);
    io.event(E::SeekFailed, 240);
    io.event(E::ReadBegin, 250);
    io.event(E::ReadFailed, 300);
    require(io.failures == 2 && io.seeks == 1 && io.reads == 1);
    require(io.seek_us == 72 && io.read_us == 125 && io.phase == 0);
    psp::FrameTimings frames;
    psp::FrameTimings::Snapshot values{};
    require(!frames.read(values));
    std::atomic<bool> done{false};
    std::thread writer([&] {
        for (uint32_t n = 1; n <= 100000; ++n) {
            psp::FrameTimings::Snapshot f;
            f.fill(n);
            frames.publish(f);
        }
        done = true;
    });
    do {
        if (frames.read(values))
            for (auto v : values) require(v == values[0]);
    } while (!done);
    writer.join();
    require(frames.read(values) && values[0] == 100000);
    frames.publish({});
    require(frames.read(values) && values[0] == 0);
    std::puts("PSP timings: I/O phases, failures, timer wrap and bounded coherent snapshots passed");
}
