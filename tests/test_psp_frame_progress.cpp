#include "frame_progress.h"
#include <cstdio>
#include <cstdlib>

namespace {
void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
}
int main() {
    using psp::FramePhase;
    psp::FrameProgress progress;
    psp::StallDetector detector;
    require(!detector.observe(progress.snapshot(), 0), "idle is not monitored");
    require(!detector.observe(progress.snapshot(), 20'000'000), "long idle is not a stall");
    progress.active(true, FramePhase::GameFrame);
    auto sample = progress.snapshot();
    require(sample.active() && sample.phase() == FramePhase::GameFrame, "atomic phase/active publication");
    require(!detector.observe(sample, 100), "first active sample starts timeout");
    require(!detector.observe(sample, 5'000'099), "no early timeout");
    require(detector.observe(sample, 5'000'100), "five second stall detected");
    require(!detector.observe(sample, 20'000'100), "only one report per incident");
    progress.completed(1, true);
    sample = progress.snapshot();
    require(sample.frames == 1 && sample.raster_updates == 1, "main-owned counters published");
    require(!detector.observe(sample, 20'000'200), "recovery rearms detector");
    require(detector.observe(sample, 25'000'200), "new incident after recovery");
    progress.active(false, FramePhase::Paused);
    require(!detector.observe(progress.snapshot(), 40'000'000), "pause clears incident");
    require(!detector.observe(progress.snapshot(), 80'000'000), "paused mode never reports");
    progress.active(false, FramePhase::Loading);
    require(!detector.observe(progress.snapshot(), 100'000'000), "loading never reports");
    progress.active(true, FramePhase::GameFrame);
    sample = progress.snapshot();
    require(!detector.observe(sample, 0xfff00000u), "new activation near timer wrap");
    require(!detector.observe(sample, uint32_t(0xfff00000u + 4'999'999u)), "wrapped timer below timeout");
    require(detector.observe(sample, uint32_t(0xfff00000u + 5'000'000u)), "wrapped timer timeout");
    sample.event = 0xffffff80u | uint32_t(FramePhase::GameFrame);
    require(!detector.observe(sample, 10'000'000), "serial change is progress");
    sample.event = 0x80u | uint32_t(FramePhase::GameFrame);
    require(!detector.observe(sample, 14'000'000), "serial wrap is progress");
    require(!detector.observe(sample, 18'999'999), "serial wrap resets deadline");
    ++sample.frames;
    require(!detector.observe(sample, 19'000'000), "counter advance prevents a false incident");
    progress.completed(2, false);
    require(progress.snapshot().raster_updates == 1, "reused3D is not counted fresh");
    progress.reset_frames();
    require(progress.snapshot().frames == 0 && progress.snapshot().raster_updates == 0, "reset counters");
    std::puts("PSP frame progress: timeout, wrap, recovery, inactive modes and counters passed");
}
