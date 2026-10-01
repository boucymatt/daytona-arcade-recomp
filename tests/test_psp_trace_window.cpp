#include "trace_window.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

int main() {
    using W = psp::TraceWindow;
    using A = W::Action;
    W window;
    unsigned entries = 0, leaves = 0, traced = 0;
    for (uint64_t frame = 1; frame <= 6000; ++frame) {
        const auto action = window.advance(frame);
        entries += action == A::Enter;
        leaves += action == A::Leave;
        traced += window.active();
        assert(window.active() == (frame >= 205 && frame <= 216));
        assert(window.advance(frame) == A::None);
    }
    assert(entries == 1 && leaves == 1 && traced == 12);
    assert(window.advance(205) == A::None); // Repeated counters never re-arm.
    window = {}; // Explicit game reset starts a new window.
    assert(window.advance(210) == A::Enter);
    assert(window.advance(UINT64_MAX) == A::Leave);
    W late;
    assert(late.advance(217) == A::None && !late.active());
    std::puts("PSP bounded trace window tests passed");
}
