// Synthetic source only: file-reader path must preserve the resident mixer.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "native_sample_mixer.h"
#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <vector>

struct Source {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(0x400000);
    mutable size_t reads = 0;
    bool fail = false;
    static uint8_t read(const void* context, uint32_t offset) {
        const auto& self = *static_cast<const Source*>(context);
        if (self.fail) throw std::runtime_error("synthetic storage failure");
        assert(offset < self.bytes.size());
        ++self.reads;
        return self.bytes[offset];
    }
};

int main() {
    Source source;
    // Signed8 sample: eight frames in banked data at logical 0x100100.
    const uint8_t header[] = {0x10, 1, 0, 0, 0, 0xff, 0xf8, 0, 0, 0, 0, 0};
    std::memcpy(source.bytes.data(), header, sizeof header);
    // Packed12 sample: same location, four frames occupying six bytes.
    std::memcpy(source.bytes.data() + 12, header, sizeof header);
    source.bytes[12] |= 0x40;
    source.bytes[18] = 0xfc;
    for (unsigned bank = 0; bank < 4; ++bank)
        for (unsigned n = 0; n < 8; ++n)
            source.bytes[bank * 0x100000 + 0x100 + n] = uint8_t(23 * n + 31 * bank);
    using Bank = snd::NativeSampleBank;
    using Mixer = snd::NativeSampleMixer;
    Bank missing;
    assert(!missing.load(Source::read, nullptr, source.bytes.size(), 0));
    assert(!missing.load(nullptr, &source, source.bytes.size(), 0));
    for (unsigned bank = 0; bank < 4; ++bank) {
        Bank resident(source.bytes.data(), source.bytes.size(), bank), paged;
        assert(paged.load(Source::read, &source, source.bytes.size(), bank));
        assert(paged.valid_sample_count() == resident.valid_sample_count());
        for (unsigned sample = 0; sample < 2; ++sample) {
            for (unsigned frame = 0; frame < 12; ++frame)
                assert(paged.value(sample, frame) == resident.value(sample, frame));
            for (float pan : {-1.f, 0.f, 0.37f, 1.f}) {
                Mixer a, b;
                Mixer::VoiceParams p;
                p.pan = pan; p.pitch = 0.73;
                assert(a.note_on(0, resident, sample, p));
                assert(b.note_on(0, paged, sample, p));
                std::array<float, 2048> x{}, y{};
                a.render(x.data(), 1024); b.render(y.data(), 1024);
                assert(std::memcmp(x.data(), y.data(), sizeof x) == 0);
                assert(a.stats().limited_frames == b.stats().limited_frames);
            }
        }
    }
    Bank paged;
    assert(paged.load(Source::read, &source, source.bytes.size(), 0));
    Mixer mixer;
    assert(mixer.note_on(0, paged, 0, {}));
    source.fail = true;
    bool caught = false;
    std::array<float, 2> out{};
    try { mixer.render(out.data(), 1); }
    catch (const std::runtime_error&) { caught = true; }
    assert(caught); // I/O must reach the frontend fault handler, not terminate.
    assert(source.reads > 0);
}
