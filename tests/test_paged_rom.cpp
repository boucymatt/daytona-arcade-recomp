// Synthetic data only: eviction, byte ordering, image wrapping and failed I/O.
#include "runtime/paged_rom.h"
#include "runtime/geo.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
struct Events {
    using E = rt::PagedRom::IoEvent;
    std::array<E, 32> values{};
    unsigned count = 0;
    static void collect(void* context, E event) noexcept {
        auto& self = *static_cast<Events*>(context);
        assert(self.count < self.values.size());
        self.values[self.count++] = event;
    }
};
struct Fixture {
    std::string path;
    std::vector<uint8_t> bytes;
    Fixture() : bytes(4 * rt::PagedRom::kPageBytes) {
        char name[] = "/tmp/daytona-paged-rom-XXXXXX";
        const int descriptor = mkstemp(name);
        if (descriptor < 0) throw std::runtime_error("mkstemp failed");
        path = name;
        for (size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = uint8_t((i * 29 + (i >> 8) * 7 + (i >> 12) * 31) ^ (i >> 3));
        std::FILE *file = fdopen(descriptor, "wb");
        if (!file) { close(descriptor); unlink(path.c_str()); throw std::runtime_error("fdopen failed"); }
        const size_t written = std::fwrite(bytes.data(), 1, bytes.size(), file);
        const int closed = std::fclose(file);
        if (written != bytes.size() || closed) { unlink(path.c_str()); throw std::runtime_error("fixture write failed"); }
    }
    ~Fixture() { unlink(path.c_str()); }
    uint32_t value(uint32_t address, unsigned count) const {
        uint32_t result = 0;
        for (unsigned b = 0; b < count; ++b) result |= uint32_t(bytes[(address + b) & (bytes.size() - 1)]) << (b * 8);
        return result;
    }
};
template<class F> void fails(F &&f) {
    bool failed = false;
    try { f(); } catch (const std::exception &) { failed = true; }
    assert(failed);
}
}

int main() {
    Fixture fixture;
    for (size_t cache : {size_t(4096), size_t(8192), size_t(12288), size_t(16384)}) {
        rt::PagedRom rom(fixture.path, uint32_t(fixture.bytes.size()), cache);
        assert(rom.size() == fixture.bytes.size() && rom.cache_bytes() == cache);
        for (unsigned cycle = 0; cycle < 3; ++cycle) {
            for (uint32_t address = 0; address < fixture.bytes.size() * 2; ++address) {
                assert(rom.read8(address) == fixture.value(address, 1));
                assert(rom.read16(address) == fixture.value(address, 2));
                assert(rom.read32(address) == fixture.value(address, 4));
            }
        }
        for (uint32_t address : {0xfffffffdu, 0xfffffffeu, 0xffffffffu}) {
            assert(rom.read16(address) == fixture.value(address, 2));
            assert(rom.read32(address) == fixture.value(address, 4));
        }
        assert(rom.stats().misses && rom.stats().hits);
        assert(rom.stats().bytes_read == rom.stats().misses * rt::PagedRom::kPageBytes);
        rt::GeoPtr words{nullptr, rom.size() / 4, rom.size() / 4 - 1, &rom};
        assert(!words.null());
        assert(*words++ == fixture.value(rom.size() - 4, 4));
        assert(*words == fixture.value(0, 4));
        fails([&] { words.write(123); });
        rt::GeoPtr16 halves{nullptr, rom.size() / 2, rom.size() / 2 - 1, &rom};
        assert(*halves++ == fixture.value(rom.size() - 2, 2));
        assert(*halves == fixture.value(0, 2));
    }
    {
        Events events;
        rt::PagedRom rom(fixture.path, 16384, 4096);
        rom.set_io_observer(Events::collect, &events);
        assert(rom.read8(0) == fixture.bytes[0]);
        assert(events.count == 4);
        assert(events.values[0] == Events::E::SeekBegin && events.values[1] == Events::E::SeekEnd);
        assert(events.values[2] == Events::E::ReadBegin && events.values[3] == Events::E::ReadEnd);
        assert(rom.read8(1) == fixture.bytes[1] && events.count == 4);
        rom.set_io_observer(nullptr, nullptr);
        assert(rom.read8(4096) == fixture.bytes[4096] && events.count == 4);
    }
    uint32_t writable[4]{};
    rt::GeoPtr ram{writable, 4, 5};
    ram.write(0x12345678);
    assert(writable[1] == 0x12345678 && *ram == 0x12345678);
    fails([&] { rt::PagedRom r(fixture.path, 12345, 4096); });
    fails([&] { rt::PagedRom r(fixture.path, 8192, 4096); });
    fails([&] { rt::PagedRom r(fixture.path, 16384, 0); });
    fails([&] { rt::PagedRom r(fixture.path, 16384, 4097); });
    fails([&] { rt::PagedRom r(fixture.path, 16384, 32768); });
    fails([&] { rt::PagedRom r(fixture.path + ".missing", 16384, 4096); });
    {
        Events events;
        rt::PagedRom rom(fixture.path, 16384, 4096);
        assert(rom.read8(0) == fixture.bytes[0]);
        rom.set_io_observer(Events::collect, &events);
        assert(truncate(fixture.path.c_str(), 17) == 0);
        fails([&] { (void)rom.read32(4096); });
        fails([&] { (void)rom.read32(4096); });
        assert(events.count == 8 && events.values[3] == Events::E::ReadFailed &&
               events.values[7] == Events::E::ReadFailed);
        // A failed eviction invalidated the old tag. This must re-read and
        // reject the truncated page, not return stale or half-written bytes.
        fails([&] { (void)rom.read8(0); });
    }
    std::puts("paged ROM: endian, eviction, wrap, read-only cursors and I/O failure checks passed");
}
