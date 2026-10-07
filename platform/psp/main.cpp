#include "controls.h"
#include "diagnostic_log.h"
#include "frame_progress.h"
#include "native_audio.h"
#include "performance_profile.h"
#include "rom_loader.h"

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psppower.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>
#include "trace_window.h"
#include <malloc.h>
#include <memory>
#include <new>
#include <stdexcept>
#include <unistd.h>

PSP_MODULE_INFO("Daytona Recomp PSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_MAIN_THREAD_PRIORITY(0x20);
PSP_MAIN_THREAD_STACK_SIZE_KB(256);
PSP_HEAP_SIZE_KB(-1);
PSP_HEAP_THRESHOLD_SIZE_KB(1024);

namespace {
std::atomic<uint32_t> running{1};
psp::FrameProgress frame_progress;
using Phase = psp::FramePhase;
psp::DiagnosticLog diagnostic_log;
bool diagnostic_enabled = false, targeted_trace = false;
psp::TraceWindow trace_window;
std::array<psp::IoCounters, 7> rom_io;
psp::FrameTimings frame_timings;
std::atomic<uint32_t> exit_reason{0}; // 1=system callback, 2=menu, 3=smoke, 4=smoke fault.
void rom_io_event(void* context, rt::PagedRom::IoEvent event) noexcept {
    static_cast<psp::IoCounters*>(context)->event(event, sceKernelGetSystemTimeLow());
}
std::array<uint32_t, 3> main_io_totals() {
    std::array<uint32_t, 3> result{};
    for (unsigned i = 0; i < 5; ++i) {
        result[0] += rom_io[i].seek_us.load();
        result[1] += rom_io[i].read_us.load();
        result[2] += rom_io[i].reads.load();
    }
    return result;
}
// Written by main; the observer never calls mallinfo on the live game heap.
struct ResourceSnapshot {
    std::atomic<uint32_t> used{0}, free{0}, frame{0};
    void publish() {
        const auto heap = mallinfo();
        used.store(uint32_t(heap.uordblks));
        free.store(uint32_t(heap.fordblks));
        frame.store(frame_progress.snapshot().frames);
    }
} resources;
void checkpoint(const char* label) {
    resources.publish();
    if (!diagnostic_enabled) return;
    const auto progress = frame_progress.snapshot();
    char text[768];
    const int length = std::snprintf(text, sizeof(text),
        "PSP checkpoint=%.240s time_low_us=%lu phase=%s frames=%lu exit_reason=%lu\n"
        "heap_used=%lu heap_free=%lu system_free=%lu main_stack_free=%d\n\n",
        label, (unsigned long)sceKernelGetSystemTimeLow(), psp::phase_name(progress.phase()),
        (unsigned long)progress.frames, (unsigned long)exit_reason.load(), (unsigned long)resources.used.load(),
        (unsigned long)resources.free.load(), (unsigned long)sceKernelTotalFreeMemSize(),
        sceKernelGetThreadStackFreeSize(sceKernelGetThreadId()));
    if (length > 0 && size_t(length) < sizeof(text))
        diagnostic_log.write(false, text, size_t(length));
}
void trace_render(void*, const char* stage, size_t progress, size_t total) {
    // Bounded detail only for the frame that failed on physical hardware.
    if (!std::strcmp(stage, "raster_batch") && progress >= 8192) return;
    char label[192];
    std::snprintf(label, sizeof(label), "render_%s target_frame=211 progress=%lu %s=%lu",
                  stage, (unsigned long)progress, !std::strncmp(stage, "poly_", 5) ? "source_index" : "polygons",
                  (unsigned long)total);
    checkpoint(label);
    if (diagnostic_log.error()) throw std::runtime_error("Render trace write failed");
}
void trace_stage(void* context, rt::GameLoop::Stage stage, uint64_t frame, uint32_t pc, uint64_t instructions) {
    if (!trace_window.active()) return;
    using S = rt::GameLoop::Stage;
    auto* video = static_cast<rt::Video*>(context);
    if (stage == S::VideoBegin) video->set_render_observer(frame == 211 ? trace_render : nullptr, nullptr);
    if (stage == S::VideoEnd) video->set_render_observer(nullptr, nullptr);
    const char* name = "unknown";
    switch (stage) {
    case S::CoreBegin: name = "core_begin"; break;
    case S::GeometryBegin: name = "geometry_begin"; break;
    case S::GeometryEnd: name = "geometry_end"; break;
    case S::VideoBegin: name = "video_begin"; break;
    case S::VideoEnd: name = "video_end"; break;
    case S::FrameEnd: name = "frame_end"; break;
    }
    char label[160];
    std::snprintf(label, sizeof(label), "trace_%s target_frame=%llu guest_pc=%08lx instructions=%llu",
                  name, (unsigned long long)frame, (unsigned long)pc, (unsigned long long)instructions);
    checkpoint(label); // Watchdog is joined throughout this bounded window.
    if (diagnostic_log.error()) throw std::runtime_error("Targeted trace write failed");
}
alignas(64) unsigned int display_list[4096];
constexpr unsigned kWidth = 480, kHeight = 272;
constexpr unsigned kStride = 512, kDisplayBytes = kStride * kHeight * 2;
static_assert(rt::Video::OutputW == int(kWidth) && rt::Video::OutputH == int(kHeight),
              "PSP must be built with native-resolution video enabled");
constexpr unsigned kTextureOffset = 2 * kDisplayBytes, kTextureBytes = 512 * 512 * 2;
static_assert(kTextureOffset + kTextureBytes <= 2 * 1024 * 1024);
static_assert(uint32_t(psp::Cross) == PSP_CTRL_CROSS && uint32_t(psp::Select) == PSP_CTRL_SELECT && uint32_t(psp::R) == PSP_CTRL_RTRIGGER);
static_assert(psp::argb_to_565(0xffff0000) == 31 && psp::argb_to_565(0xff0000ff) == 0xf800);

uint64_t now_us() { return uint64_t(sceKernelGetSystemTimeWide()); }
int exit_callback(int, int, void*) { exit_reason.store(1); running.store(0); return 0; }
int callback_thread(SceSize, void*) {
    const int callback = sceKernelCreateCallback("daytona_exit", exit_callback, nullptr);
    if (callback >= 0) sceKernelRegisterExitCallback(callback);
    while (running.load()) sceKernelSleepThreadCB();
    if (callback >= 0) sceKernelDeleteCallback(callback);
    return 0;
}

class Display {
public:
    Display() {
        vram_ = static_cast<uint8_t*>(sceGeEdramGetAddr());
        uncached_ = reinterpret_cast<uint8_t*>(uintptr_t(vram_) | 0x40000000u);
        std::memset(uncached_, 0, kTextureOffset + kTextureBytes);
        sceGuInit();
        sceGuStart(GU_DIRECT, display_list);
        sceGuDrawBuffer(GU_PSM_5650, nullptr, kStride);
        sceGuDispBuffer(480, 272, reinterpret_cast<void*>(kDisplayBytes), kStride);
        sceGuOffset(2048 - 240, 2048 - 136);
        sceGuViewport(2048, 2048, 480, 272);
        sceGuScissor(0, 0, 480, 272); sceGuEnable(GU_SCISSOR_TEST);
        sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_BLEND);
        sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT);
        finish(); sceDisplayWaitVblankStart(); sceGuDisplay(GU_TRUE);
        pspDebugScreenInitEx(uncached_, GU_PSM_5650, 0);
    }
    ~Display() { sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE); sceGuDisplay(GU_FALSE); sceGuTerm(); }
    void text_screen() {
        sceGuStart(GU_DIRECT, display_list);
        sceGuClearColor(0xff18100a); sceGuClear(GU_COLOR_BUFFER_BIT); finish();
        pspDebugScreenSetOffset(int(draw_offset_));
        pspDebugScreenSetBackColor(0xff18100a); pspDebugScreenSetTextColor(0xffeeeeee);
        pspDebugScreenSetXY(0, 1);
    }
    void present_text() { swap(); }
    void present(const std::vector<uint32_t>& screen) {
        if (screen.size() != size_t(kWidth) * kHeight) throw std::runtime_error("Invalid CPU framebuffer size");
        // The prior draw is complete before this sole texture is modified.
        frame_progress.mark(Phase::TextureWait);
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        frame_progress.mark(Phase::TextureUpload);
        auto* pixels = reinterpret_cast<uint16_t*>(uncached_ + kTextureOffset);
        for (unsigned y = 0; y < kHeight; ++y) {
            auto* row = pixels + y * kStride;
            for (unsigned x = 0; x < kWidth; ++x) row[x] = psp::argb_to_565(screen[y * kWidth + x]);
            std::fill(row + kWidth, row + kStride, row[kWidth - 1]);
        }
        std::memcpy(pixels + kHeight * kStride, pixels + (kHeight - 1) * kStride, kStride * sizeof(uint16_t));
        frame_progress.mark(Phase::GuBuild);
        sceGuStart(GU_DIRECT, display_list);
        sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT);
        sceGuEnable(GU_TEXTURE_2D); sceGuTexMode(GU_PSM_5650, 0, 0, 0);
        sceGuTexImage(0, 512, 512, 512, vram_ + kTextureOffset);
        sceGuTexFlush(); sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
        // The CPU target already includes the selected aspect/letterbox. Map it
        // 1:1 to the physical LCD, with no second downscale or upscaling.
        sceGuTexFilter(GU_NEAREST, GU_NEAREST); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        struct Vertex { float u, v, x, y, z; };
        // Narrow strips fit the GE texture cache and retain the same mapping.
        for (unsigned left = 0; left < kWidth; left += 32) {
            const unsigned right = std::min(left + 32, kWidth);
            auto* vertices = static_cast<Vertex*>(sceGuGetMemory(2 * sizeof(Vertex)));
            vertices[0] = {float(left), 0, float(left), 0, 0};
            vertices[1] = {float(right), float(kHeight), float(right), float(kHeight), 0};
            sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, nullptr, vertices);
        }
        sceGuDisable(GU_TEXTURE_2D); finish(); swap();
    }
private:
    uint8_t* vram_ = nullptr;
    uint8_t* uncached_ = nullptr;
    uintptr_t draw_offset_ = 0;
    void finish() {
        frame_progress.mark(Phase::GuFinish); sceGuFinish();
        frame_progress.mark(Phase::GuWait); sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    }
    void swap() {
        frame_progress.mark(Phase::Vblank); sceDisplayWaitVblankStart();
        frame_progress.mark(Phase::Swap); draw_offset_ = reinterpret_cast<uintptr_t>(sceGuSwapBuffers());
    }
};

// Only frontend-owned save files are replaced. Keep the previous complete
// copy if the PSP filesystem refuses an overwrite rename or a later write fails.
bool replace_save(const char* temporary, const char* path) {
    if (std::rename(temporary, path) == 0) return true;
    FILE* existing = std::fopen(path, "rb");
    if (!existing) return false;
    std::fclose(existing);
    char backup[96]; std::snprintf(backup, sizeof(backup), "%s.bak", path);
    std::remove(backup);
    if (std::rename(path, backup) != 0) return false;
    if (std::rename(temporary, path) == 0) return true;
    std::rename(backup, path); // If restore fails, the .bak is still recoverable.
    return false;
}
template<class Bytes> bool read_nv(const char* path, Bytes& bytes) {
    FILE* file = std::fopen(path, "rb"); if (!file) return false;
    std::array<uint8_t, 0x4000> scratch;
    const bool size_ok = bytes.size() <= scratch.size() && std::fseek(file, 0, SEEK_END) == 0 &&
        std::ftell(file) == long(bytes.size()) && std::fseek(file, 0, SEEK_SET) == 0;
    const bool ok = size_ok && std::fread(scratch.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    if (ok) std::copy_n(scratch.data(), bytes.size(), bytes.data());
    return ok;
}
template<class Bytes> void load_nv(const char* path, Bytes& bytes) {
    if (read_nv(path, bytes)) return;
    char backup[96]; std::snprintf(backup, sizeof(backup), "%s.bak", path); read_nv(backup, bytes);
}
template<class Bytes> bool save_nv(const char* path, const Bytes& bytes) {
    char temporary[96]; std::snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    FILE* file = std::fopen(temporary, "wb"); if (!file) return false;
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    const bool flushed = std::fflush(file) == 0;
    const bool closed = std::fclose(file) == 0;
    return written && flushed && closed && replace_save(temporary, path);
}
bool save_cabinet(rt::GameLoop* game) {
    if (!game) return true;
    const bool a = save_nv("ioboard_eeprom.bin", game->board().io().eeprom);
    const bool b = save_nv("backup_ram.bin", game->board().backup_ram());
    if (a) game->board().io().eeprom_dirty = false;
    return a && b;
}

struct Settings {
    int volume = 80, mute = 0, stretch = 0, display_skip = 0;
    void load() {
        FILE* file = std::fopen("psp-settings.ini", "r");
        if (!file) return;
        char key[32]; int value;
        while (std::fscanf(file, " %31[^=]=%d", key, &value) == 2) {
            if (!std::strcmp(key, "volume")) volume = std::clamp(value, 0, 100);
            else if (!std::strcmp(key, "mute")) mute = value != 0;
            else if (!std::strcmp(key, "stretch")) stretch = value != 0;
            else if (!std::strcmp(key, "display_skip")) display_skip = std::clamp(value, 0, 3);
        }
        std::fclose(file);
    }
    bool save() const {
        FILE* file = std::fopen("psp-settings.tmp", "w");
        if (!file) return false;
        const bool written = std::fprintf(file, "volume=%d\nmute=%d\nstretch=%d\ndisplay_skip=%d\n",
                                          volume, mute, stretch, display_skip) > 0;
        const bool closed = std::fclose(file) == 0;
        return written && closed && replace_save("psp-settings.tmp", "psp-settings.ini");
    }
};
void stage(Display& display, const char* message) {
    checkpoint(message); // Main is the sole writer while the observer is stopped.
    const auto heap = mallinfo();
    std::printf("PSP stage=%s heap_used=%lu heap_free=%lu system_free=%lu\n", message,
                (unsigned long)heap.uordblks, (unsigned long)heap.fordblks,
                (unsigned long)sceKernelTotalFreeMemSize()); std::fflush(stdout);
    display.text_screen(); pspDebugScreenPrintf("  DAYTONA RECOMP - PSP\n\n  %s\n\n", message);
    pspDebugScreenPrintf("  Heap arena used/free: %lu/%lu KiB\n", (unsigned long)heap.uordblks / 1024,
                        (unsigned long)heap.fordblks / 1024);
    display.present_text();
}
unsigned smoke_frames(unsigned& skip, unsigned& stall_ms) {
    FILE* file = std::fopen("smoke.txt", "r"); if (!file) return 0;
    unsigned frames = 120; skip = 0;
    std::fscanf(file, "%u %u %u", &frames, &skip, &stall_ms); std::fclose(file);
    stall_ms = std::min(stall_ms, 10000u); // Optional, bounded observer fault injection.
    skip = std::min(skip, 3u); return std::clamp(frames, 1u, 6000u);
}
using Audio = psp::NativeAudio<snd::NativeSoundEngine>;

class StallWatchdog {
public:
    explicit StallWatchdog(const Audio& audio) : audio_(audio) {}
    ~StallWatchdog() { stop(); }
    bool start() {
        if (!diagnostic_log.ready()) { error_ = uint32_t(diagnostic_log.error()); return false; }
        main_thread_ = sceKernelGetThreadId();
        if (main_thread_ < 0) { error_ = uint32_t(main_thread_); return false; }
        thread_ = sceKernelCreateThread("daytona_stall", entry, 0x10, 16 * 1024, PSP_THREAD_ATTR_USER, nullptr);
        if (thread_ < 0) { error_ = uint32_t(thread_); return false; }
        stop_.store(0); finished_.store(0);
        auto* self = this;
        const int result = sceKernelStartThread(thread_, sizeof(self), &self);
        if (result < 0) {
            error_ = uint32_t(result);
            const int removed = sceKernelDeleteThread(thread_);
            if (removed < 0) error_ = uint32_t(removed);
            thread_ = -1; finished_.store(1); return false;
        }
        return true;
    }
    void stop() {
        stop_.store(1, std::memory_order_release);
        if (thread_ < 0) return;
        const int joined = sceKernelWaitThreadEnd(thread_, nullptr);
        if (joined < 0) {
            error_ = uint32_t(joined);
            // Never destroy references still in use, even if the join API
            // unexpectedly fails. No forced termination inside filesystem I/O.
            while (!finished_.load(std::memory_order_acquire)) sceKernelDelayThread(1000);
        }
        const int removed = sceKernelDeleteThread(thread_);
        if (removed < 0) error_ = uint32_t(removed);
        thread_ = -1;
    }
    void audio_thread(SceUID thread) { audio_thread_.store(thread, std::memory_order_release); }
    uint32_t error() const { return error_.load(std::memory_order_relaxed); }
    uint32_t incidents() const { return incidents_.load(std::memory_order_relaxed); }
    bool available() const { return thread_ >= 0 && !finished_.load(std::memory_order_acquire); }
private:
    const Audio& audio_; // Constructed first; joined before audio destruction.
    SceUID thread_ = -1, main_thread_ = -1;
    std::atomic<SceUID> audio_thread_{-1};
    std::atomic<uint32_t> stop_{1}, finished_{1}, error_{0}, incidents_{0};
    static int entry(SceSize, void* argument) {
        return (*static_cast<StallWatchdog**>(argument))->run();
    }
    int run() noexcept {
        psp::StallDetector detector;
        uint32_t last_record = sceKernelGetSystemTimeLow();
        bool periodic_failed = false;
        while (!stop_.load(std::memory_order_acquire)) {
            const auto snapshot = frame_progress.snapshot();
            const uint32_t now = sceKernelGetSystemTimeLow();
            if (detector.observe(snapshot, now)) {
                incidents_.fetch_add(1, std::memory_order_relaxed);
                record(snapshot, now, detector.stalled_us(now), true);
            }
            if (diagnostic_enabled && !periodic_failed && snapshot.active() &&
                uint32_t(now - last_record) >= 1'000'000) {
                periodic_failed = !record(snapshot, now, 0, false);
                // Write/close time counts as overhead, not another due record.
                // Heartbeats do not force a whole-device sync during ROM reads.
                last_record = sceKernelGetSystemTimeLow();
            }
            const int delayed = sceKernelDelayThread(250000);
            if (delayed < 0) { error_ = uint32_t(delayed); break; }
        }
        finished_.store(1, std::memory_order_release);
        return 0;
    }
    bool record(psp::ProgressSnapshot snapshot, uint32_t now, uint32_t stalled, bool stall) noexcept {
        SceKernelThreadInfo info{};
        info.size = sizeof(info);
        const int status = sceKernelReferThreadStatus(main_thread_, &info);
        const auto sound = audio_.stats();
        const auto audio_thread = audio_thread_.load(std::memory_order_acquire);
        SceKernelThreadInfo audio_info{}; audio_info.size = sizeof(audio_info);
        const int audio_status = audio_thread >= 0 ? sceKernelReferThreadStatus(audio_thread, &audio_info) : -1;
        const int audio_stack = audio_thread >= 0 ? sceKernelGetThreadStackFreeSize(audio_thread) : -1;
        // Only fixed-size local storage, kernel status, and published atomics.
        // No GU calls, game/Video/heap reads, or shared libc FILE locks.
        char text[4096];
        int length = std::snprintf(text, sizeof(text),
            "PSP %s time_low_us=%lu stalled_us=%lu phase=%s event=%08lx\n"
            "completed_frames=%lu fresh_3d_updates=%lu main_thread=%d status_result=%08lx\n"
            "thread_status=%08lx wait_type=%d wait_id=%d priority=%d run_clocks=%08lx%08lx\n"
            "audio_blocks=%lu audio_frames=%lu queued=%lu voices=%lu failed=%lu system_error=%08lx\n"
            "audio_render_us=%lu peak_render_us=%lu late_blocks=%lu output_call_us=%lu\n"
            "fairness_yields=%lu fairness_delay_us=%lu\n"
            "main_stack_free=%d main_stack_size=%lu observer_stack_free=%d\n"
            "audio_thread=%d audio_status_result=%08lx audio_status=%08lx audio_wait_type=%d audio_wait_id=%d\n"
            "audio_stack_free=%d audio_stack_size=%lu\n"
            "heap_used=%lu heap_free=%lu heap_sample_frame=%lu system_free=%lu largest_block=%lu\n\n",
            stall ? "no_main_progress" : "heartbeat", (unsigned long)now, (unsigned long)stalled,
            psp::phase_name(snapshot.phase()), (unsigned long)snapshot.event,
            (unsigned long)snapshot.frames, (unsigned long)snapshot.raster_updates, int(main_thread_), (unsigned long)uint32_t(status),
            (unsigned long)uint32_t(info.status), info.waitType, int(info.waitId), info.currentPriority,
            (unsigned long)info.runClocks.hi, (unsigned long)info.runClocks.low,
            (unsigned long)sound.blocks, (unsigned long)sound.frames, (unsigned long)sound.queued,
            (unsigned long)sound.voices, (unsigned long)sound.failed, (unsigned long)sound.system_error,
            (unsigned long)sound.render_us, (unsigned long)sound.peak_render_us, (unsigned long)sound.late_blocks,
            (unsigned long)sound.output_call_us, (unsigned long)sound.fairness_yields, (unsigned long)sound.fairness_delay_us,
            sceKernelGetThreadStackFreeSize(main_thread_), (unsigned long)info.stackSize,
            sceKernelGetThreadStackFreeSize(sceKernelGetThreadId()), int(audio_thread), (unsigned long)uint32_t(audio_status),
            (unsigned long)uint32_t(audio_info.status), audio_info.waitType, int(audio_info.waitId),
            audio_stack, (unsigned long)audio_info.stackSize,
            (unsigned long)resources.used.load(), (unsigned long)resources.free.load(),
            (unsigned long)resources.frame.load(), (unsigned long)sceKernelTotalFreeMemSize(),
            (unsigned long)sceKernelMaxFreeMemSize());
        if (length < 0 || size_t(length) >= sizeof(text)) { error_ = uint32_t(-1); return false; }
        if (diagnostic_enabled) {
            auto append = [&](const char* format, auto... args) {
                if (length < 0) return;
                const int added = std::snprintf(text + length, sizeof(text) - size_t(length), format, args...);
                if (added < 0 || size_t(added) >= sizeof(text) - size_t(length)) length = -1;
                else length += added;
            };
            psp::FrameTimings::Snapshot f{};
            using T = psp::FrameTimings;
            const bool valid = frame_timings.read(f) && f[T::Frame] != 0;
            append("profile_valid=%d profile_frame=%lu board_wall_us=%lu core_wall_us=%lu geometry_wall_us=%lu video_wall_us=%lu\n"
                   "raster_wall_us=%lu tile_cache_wall_us=%lu tile_draw_wall_us=%lu composite_wall_us=%lu present_wall_us=%lu\n"
                   "main_rom_seek_wall_us=%lu main_rom_read_wall_us=%lu main_rom_pages=%lu tiles_rebuilt=%lu\n",
                   int(valid), (unsigned long)f[T::Frame], (unsigned long)f[T::Board], (unsigned long)f[T::Core],
                   (unsigned long)f[T::Geometry], (unsigned long)f[T::Video], (unsigned long)f[T::Raster],
                   (unsigned long)f[T::TileCache], (unsigned long)f[T::TileDraw], (unsigned long)f[T::Composite],
                   (unsigned long)f[T::Present], (unsigned long)f[T::RomSeek], (unsigned long)f[T::RomRead],
                   (unsigned long)f[T::RomPages], (unsigned long)f[T::TilesRebuilt]);
            for (unsigned i = 0; i < rom_io.size(); ++i) {
                const auto& io = rom_io[i];
                const auto phase = io.phase.load();
                append("rom=%s io_phase=%lu active_wall_us=%lu seeks_total=%lu pages_total=%lu seek_wall_us_mod32=%lu read_wall_us_mod32=%lu failures_total=%lu\n",
                       psp::kRomNames[i], (unsigned long)phase,
                       (unsigned long)(phase ? uint32_t(sceKernelGetSystemTimeLow() - io.began.load()) : 0),
                       (unsigned long)io.seeks.load(), (unsigned long)io.reads.load(),
                       (unsigned long)io.seek_us.load(), (unsigned long)io.read_us.load(),
                       (unsigned long)io.failures.load());
            }
            append("\n");
            if (length < 0) { error_ = uint32_t(-1); return false; }
        }
        const bool ok = diagnostic_log.write(stall, text, size_t(length), stall);
        if (!ok) error_ = uint32_t(diagnostic_log.error());
        return ok;
    }
};
void report_smoke(const char* result, rt::GameLoop* game, const Audio& audio, const StallWatchdog& watchdog, uint64_t elapsed) {
    const auto stats = audio.stats(); const auto heap = mallinfo();
    const uint64_t hash = game ? game->board().video().screen_hash() : 0;
    FILE* file = std::fopen("smoke-result.txt", "w");
    if (file) {
        std::fprintf(file, "status=%s\nframes=%llu\nscreen_hash=%016llx\nelapsed_us=%llu\n"
                     "heap_used=%lu\nheap_free=%lu\nsystem_free=%lu\naudio_blocks=%lu\naudio_failed=%lu\n"
                     "audio_system_error=%08lx\naudio_invalid=%lu\naudio_unsupported=%lu\n",
                     result, (unsigned long long)(game ? game->frames() : 0), (unsigned long long)hash,
                     (unsigned long long)elapsed, (unsigned long)heap.uordblks, (unsigned long)heap.fordblks,
                     (unsigned long)sceKernelTotalFreeMemSize(), (unsigned long)stats.blocks,
                     (unsigned long)stats.failed, (unsigned long)stats.system_error,
                     (unsigned long)stats.invalid, (unsigned long)stats.unsupported);
        std::fprintf(file, "render_width=%u\nrender_height=%u\ncpu_mhz=%d\nbus_mhz=%d\nvram_bytes=%u\n",
            kWidth, kHeight, scePowerGetCpuClockFrequencyInt(), scePowerGetBusClockFrequencyInt(),
            kTextureOffset + kTextureBytes);
        std::fprintf(file, "audio_render_us=%lu\naudio_peak_render_us=%lu\naudio_late_blocks=%lu\n",
            (unsigned long)stats.render_us, (unsigned long)stats.peak_render_us, (unsigned long)stats.late_blocks);
        const auto completed = frame_progress.snapshot();
        std::fprintf(file, "audio_output_call_us=%lu\naudio_fairness_yields=%lu\naudio_fairness_delay_us=%lu\nfresh_3d_updates=%lu\n",
            (unsigned long)stats.output_call_us, (unsigned long)stats.fairness_yields,
            (unsigned long)stats.fairness_delay_us, (unsigned long)completed.raster_updates);
        std::fprintf(file, "watchdog_available=%d\nwatchdog_error=%08lx\nwatchdog_incidents=%lu\n",
            int(watchdog.available()), (unsigned long)watchdog.error(), (unsigned long)watchdog.incidents());
        std::fprintf(file, "diagnostic_enabled=%d\ndiagnostic_records=%lu\ndiagnostic_error=%08lx\n",
            int(diagnostic_enabled), (unsigned long)diagnostic_log.records(), (unsigned long)uint32_t(diagnostic_log.error()));
        std::fclose(file);
    }
    std::printf("PSP smoke=%s frames=%llu screen_hash=%016llx audio_failed=%lu\n", result,
                (unsigned long long)(game ? game->frames() : 0), (unsigned long long)hash,
                (unsigned long)stats.failed); std::fflush(stdout);
}
} // namespace

int main() {
    // newlib maintains a process cwd; native kernel I/O on a new thread does
    // not inherit it. Resolve absolute paths here before any worker is started.
    char cwd[1024];
    diagnostic_log.initialize(getcwd(cwd, sizeof(cwd)));
    if (FILE* enabled = std::fopen("psp-diagnostics.txt", "r")) {
        int value = 0;
        const bool parsed = std::fscanf(enabled, "%d", &value) == 1;
        diagnostic_enabled = parsed && (value == 1 || value == 2);
        targeted_trace = parsed && value == 2;
        std::fclose(enabled);
    }
    checkpoint("test12_boot_before_callbacks");
    int callbacks = sceKernelCreateThread("daytona_callbacks", callback_thread, 0x11, 4096, PSP_THREAD_ATTR_USER, nullptr);
    if (callbacks >= 0 && sceKernelStartThread(callbacks, 0, nullptr) < 0) {
        sceKernelDeleteThread(callbacks); callbacks = -1;
    }
    sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    // Official PSP maximum, not an overclock. Audio runs at its device clock.
    const int clock_result = scePowerSetClockFrequency(333, 333, 166);
    if (diagnostic_enabled) {
        char text[256];
        const int length = std::snprintf(text, sizeof(text),
            "PSP clocks result=%08lx cpu_mhz=%d bus_mhz=%d\n\n",
            (unsigned long)uint32_t(clock_result), scePowerGetCpuClockFrequencyInt(),
            scePowerGetBusClockFrequencyInt());
        if (length > 0 && size_t(length) < sizeof(text))
            diagnostic_log.write(false, text, size_t(length));
    }
    checkpoint("before_graphics");
    Display display;
    stage(display, "PSP-1000 / 32 MB initialization");
    Settings settings; settings.load();
    unsigned smoke_skip = 0, smoke_stall_ms = 0;
    const unsigned smoke = smoke_frames(smoke_skip, smoke_stall_ms);
    if (smoke) settings.display_skip = int(smoke_skip);
    psp::Controls controls;
    std::unique_ptr<rt::GameLoop> game;
    Audio audio;
    StallWatchdog watchdog(audio);
    bool menu = true, start_requested = smoke != 0;
    int selection = 0;
    uint32_t held = 0;
    char message[192] = "Start loads your imported files from roms/.";
    if (diagnostic_enabled) std::snprintf(message, sizeof(message), "Test10 profiling ON. Logs saved beside EBOOT.PBP.");
    if (diagnostic_log.error()) std::snprintf(message, sizeof(message), "Diagnostic path/write error: %08lx", (unsigned long)uint32_t(diagnostic_log.error()));
    const uint64_t boot_time = now_us();
    uint64_t deadline = boot_time;
    uint32_t board_us = 0, present_us = 0, resource_time = 0;
    constexpr uint64_t frame_us = 17384; // 656 * 424 / 16 MHz, rounded for host pacing only.
    auto apply_settings = [&] {
        audio.volume(unsigned(settings.volume)); audio.mute(settings.mute != 0);
        if (game) game->board().video().set_psp_stretch(settings.stretch != 0);
    };
    auto load_game = [&] {
        frame_progress.active(false, Phase::Loading);
        watchdog.stop(); // Main owns the journal during loading/checkpoints.
        if (diagnostic_enabled && diagnostic_log.error())
            throw std::runtime_error("Diagnostic file unavailable; gameplay not started.");
        checkpoint("before_game_load");
        frame_progress.reset_frames();
        trace_window = {};
        frame_timings.publish({});
        watchdog.audio_thread(-1);
        audio.close();
        if (!smoke && !save_cabinet(game.get())) throw std::runtime_error("Cabinet save failed before reset");
        game.reset(); controls = {};
        stage(display, "Loading checked ROM regions (paged)");
        std::array<void*, 7> io_contexts{};
        for (unsigned i = 0; i < rom_io.size(); ++i) io_contexts[i] = &rom_io[i];
        auto loaded = psp::load_game("roms", diagnostic_enabled ? rom_io_event : nullptr, io_contexts);
        stage(display, "Creating native main board and CPU renderer");
        game = std::make_unique<rt::GameLoop>(std::move(loaded.images), false);
        std::printf("PSP low memory: TGP image copy released; guest framebuffer resident=%zu/1048576 bytes\n",
                    game->board().framebuffer_resident_bytes());
        if (targeted_trace) game->set_stage_observer(trace_stage, &game->board().video());
        if (smoke || diagnostic_enabled) {
            game->set_profile_clock(now_us);
            game->board().video().set_profile_clock(now_us);
        }
        if (!smoke) {
            load_nv("ioboard_eeprom.bin", game->board().io().eeprom);
            load_nv("backup_ram.bin", game->board().backup_ram());
        }
        checkpoint("before_audio_open");
        if (!audio.open(std::move(loaded.audio))) throw std::runtime_error("Native audio engine missing");
        apply_settings(); stage(display, "Starting dedicated 48 kHz audio thread");
        if (!audio.resume()) throw std::runtime_error("PSP SRC audio/thread startup failed");
        watchdog.audio_thread(audio.worker_thread());
        stage(display, "Ready: native 480x272 framebuffer + GU");
        checkpoint("before_first_game_frame");
        if (diagnostic_enabled && diagnostic_log.error())
            throw std::runtime_error("Diagnostic write/sync failed; gameplay not started.");
        if (!watchdog.start()) {
            if (diagnostic_enabled) throw std::runtime_error("Diagnostic observer could not start.");
            std::snprintf(message, sizeof(message), "Stall diagnostics unavailable: %08lx", (unsigned long)watchdog.error());
        }
        controls.latch(held); deadline = now_us(); menu = false;
        frame_progress.active(true, Phase::Input);
    };
    while (running.load()) {
        frame_progress.mark(Phase::Input);
        SceCtrlData raw{}; sceCtrlPeekBufferPositive(&raw, 1);
        const uint32_t pressed = raw.Buttons & ~held; held = raw.Buttons;
        try {
            if (start_requested) { start_requested = false; load_game(); }
            if (!menu && psp::menu_chord(held)) {
                frame_progress.active(false, Phase::Paused);
                watchdog.audio_thread(-1);
                audio.pause(); menu = true; controls.latch(held);
                std::snprintf(message, sizeof(message), "%s", save_cabinet(game.get())
                    ? "Paused. Cabinet settings saved." : "Paused. Cabinet save failed.");
            }
            if (menu) {
                if (pressed & PSP_CTRL_UP) selection = (selection + 7) % 8;
                if (pressed & PSP_CTRL_DOWN) selection = (selection + 1) % 8;
                const int direction = int(bool(pressed & PSP_CTRL_RIGHT)) - int(bool(pressed & PSP_CTRL_LEFT));
                bool changed = false;
                if (direction) {
                    if (selection == 1) { settings.volume = std::clamp(settings.volume + direction * 5, 0, 100); changed = true; }
                    if (selection == 2) { settings.mute ^= 1; changed = true; }
                    if (selection == 3) { settings.stretch ^= 1; changed = true; }
                    if (selection == 4) { settings.display_skip = std::clamp(settings.display_skip + direction, 0, 3); changed = true; }
                }
                if (changed) {
                    apply_settings(); std::snprintf(message, sizeof(message), "%s", settings.save() ? "Options saved." : "Could not save options to Memory Stick.");
                }
                if ((pressed & PSP_CTRL_CROSS) && !psp::menu_chord(held)) {
                    if (selection == 0) {
                        if (!game) load_game();
                        else { if (!audio.resume()) throw std::runtime_error("Audio resume failed"); watchdog.audio_thread(audio.worker_thread()); menu = false; controls.latch(held); deadline = now_us(); frame_progress.active(true, Phase::Input); }
                    } else if (selection == 2 || selection == 3) {
                        if (selection == 2) settings.mute ^= 1; else settings.stretch ^= 1;
                        apply_settings(); std::snprintf(message, sizeof(message), "%s", settings.save() ? "Options saved." : "Could not save options.");
                    } else if (selection == 5) load_game();
                    else if (selection == 6) std::snprintf(message, sizeof(message), "%s", settings.save() ? "Options saved." : "Could not save options.");
                    else if (selection == 7) { exit_reason.store(2); running.store(0); }
                }
                if (!menu) continue;
                display.text_screen();
                pspDebugScreenPrintf("  DAYTONA RECOMP - PSP-1000\n  480x272 CPU renderer / native audio\n\n");
                const char* entries[] = {game ? "Resume" : "Start", "Volume", "Mute", "Aspect", "Display skip", "Reset game", "Save options", "Quit"};
                for (int i = 0; i < 8; ++i) {
                    pspDebugScreenPrintf("  %c %-16s", selection == i ? '>' : ' ', entries[i]);
                    if (i == 1) pspDebugScreenPrintf("%d%%", settings.volume);
                    if (i == 2) pspDebugScreenPrintf("%s", settings.mute ? "On" : "Off");
                    if (i == 3) pspDebugScreenPrintf("%s", settings.stretch ? "Stretch" : "4:3");
                    if (i == 4) pspDebugScreenPrintf("%d (presentation only)", settings.display_skip);
                    pspDebugScreenPrintf("\n");
                }
                const auto heap = mallinfo();
                pspDebugScreenPrintf("\n  Cross: select    Left/Right: change\n\n  Stick/D-pad: steer   Cross: accelerate\n  Square: brake       L/R: gear down/up\n  Select: coin        Start: start\n  Circle/Triangle/Up/Down: views\n  Start + Select: pause/menu\n\n  Heap arena used/free: %lu/%lu KiB\n\n  %s\n",
                    (unsigned long)heap.uordblks / 1024, (unsigned long)heap.fordblks / 1024, message);
                const auto sound = audio.stats();
                pspDebugScreenPrintf("  Audio unsupported/errors: %lu/%lu  SRC: %08lx\n", (unsigned long)sound.unsupported,
                                     (unsigned long)(sound.invalid + sound.failed), (unsigned long)sound.system_error);
                if (watchdog.error()) pspDebugScreenPrintf("  Stall diagnostics error: %08lx\n", (unsigned long)watchdog.error());
                display.present_text(); continue;
            }
            if (diagnostic_enabled && (diagnostic_log.error() || watchdog.error()))
                throw std::runtime_error("Diagnostics failed; paused to preserve existing log.");
            const uint64_t now = now_us();
            if (uint32_t(now) - resource_time >= 1'000'000) {
                resources.publish(); resource_time = uint32_t(now);
            }
            if (!smoke && now < deadline) { sceKernelDelayThread(unsigned(std::min<uint64_t>(deadline - now, 2000))); continue; }
            const auto mapped = controls.sample({held, raw.Lx});
            rt::Inputs inputs;
            if (!smoke) {
                inputs.steer = mapped.steer; inputs.accel = mapped.accel; inputs.brake = mapped.brake;
                inputs.in0 = mapped.in0; inputs.in1 = mapped.in1; inputs.in2 = mapped.in2;
            }
            if (targeted_trace) {
                const auto action = trace_window.advance(game->frames() + 1);
                if (action == psp::TraceWindow::Action::Enter) {
                    watchdog.stop(); // Single writer: main now owns the journal.
                    checkpoint("trace_window_enter");
                } else if (action == psp::TraceWindow::Action::Leave) {
                    checkpoint("trace_window_leave");
                    if (!watchdog.start()) throw std::runtime_error("Trace observer restart failed");
                }
                if (trace_window.active()) {
                    char label[192];
                    std::snprintf(label, sizeof(label),
                        "trace_input target_frame=%llu buttons=%08lx lx=%u in0=%02x in1=%02x in2=%02x steer=%u accel=%u brake=%u",
                        (unsigned long long)(game->frames() + 1), (unsigned long)held, unsigned(raw.Lx),
                        unsigned(inputs.in0), unsigned(inputs.in1), unsigned(inputs.in2),
                        unsigned(inputs.steer), unsigned(inputs.accel), unsigned(inputs.brake));
                    checkpoint(label);
                }
            }
            scePowerTick(0);
            const bool profiling = smoke || diagnostic_enabled;
            const auto io_before = diagnostic_enabled ? main_io_totals() : std::array<uint32_t, 3>{};
            const auto board_begin = profiling ? now_us() : 0;
            frame_progress.mark(Phase::GameFrame);
            // Test-only delay configured by the third smoke.txt field. No
            // instruction/frame/sample is discarded, and releases omit it.
            if (smoke && smoke_stall_ms && game->frames() == 0)
                sceKernelDelayThread(smoke_stall_ms * 1000);
            game->run_frame(inputs);
            frame_progress.completed(uint32_t(game->frames()), game->board().video().rendered_now());
            if (profiling) board_us = uint32_t(now_us() - board_begin);
            frame_progress.mark(Phase::AudioSubmit);
            const auto bytes = game->board().take_sound_bytes();
            if (!audio.send(bytes.data(), bytes.size()) || audio.stats().failed)
                throw std::runtime_error("Native audio error or command queue overflow");
            if (trace_window.active()) checkpoint("trace_audio_submitted");
            present_us = 0;
            if ((game->frames() - 1) % unsigned(settings.display_skip + 1) == 0 || (smoke && game->frames() == smoke)) {
                const auto present_begin = profiling ? now_us() : 0;
                display.present(game->screen());
                if (profiling) present_us = uint32_t(now_us() - present_begin);
            }
            if (trace_window.active()) checkpoint("trace_present_complete");
            if (diagnostic_enabled) {
                const auto after = main_io_totals();
                const auto& p = game->last_profile();
                const auto& v = game->board().video().last_profile();
                frame_timings.publish({uint32_t(game->frames()), board_us, uint32_t(p.core()),
                    uint32_t(p.geometry), uint32_t(p.video), uint32_t(v.raster), uint32_t(v.tile_cache),
                    uint32_t(v.tile_draw), uint32_t(v.composite), present_us,
                    uint32_t(after[0] - io_before[0]), uint32_t(after[1] - io_before[1]),
                    uint32_t(after[2] - io_before[2]), v.tiles_rebuilt});
            }
            deadline += frame_us;
            if (now_us() > deadline + 4 * frame_us) deadline = now_us(); // No guest instruction or game frame is omitted.
            if (smoke && (game->frames() == 1 || game->frames() % 60 == 0)) {
                const auto s = audio.stats();
                FILE* progress = std::fopen("smoke-progress.txt", "w");
                if (progress) {
                    std::fprintf(progress, "frames=%llu elapsed_us=%llu board_wall_us=%lu present_us=%lu\n"
                        "audio_blocks=%lu render_us=%lu peak_render_us=%lu late_blocks=%lu voices=%lu\n"
                        "heap_arena_used=%lu heap_arena_free=%lu\n",
                        (unsigned long long)game->frames(), (unsigned long long)(now_us() - boot_time),
                        (unsigned long)board_us, (unsigned long)present_us, (unsigned long)s.blocks,
                        (unsigned long)s.render_us, (unsigned long)s.peak_render_us, (unsigned long)s.late_blocks,
                        (unsigned long)s.voices, (unsigned long)mallinfo().uordblks, (unsigned long)mallinfo().fordblks);
                    std::fprintf(progress, "audio_output_call_us=%lu audio_fairness_yields=%lu audio_fairness_delay_us=%lu\n"
                        "watchdog_available=%d watchdog_error=%08lx watchdog_incidents=%lu\n",
                        (unsigned long)s.output_call_us, (unsigned long)s.fairness_yields, (unsigned long)s.fairness_delay_us,
                        int(watchdog.available()), (unsigned long)watchdog.error(), (unsigned long)watchdog.incidents());
                    const auto& v = game->board().video().last_profile();
                    std::fprintf(progress, "tile_build_us=%llu tile_draw_us=%llu raster_us=%llu composite_us=%llu\n",
                        (unsigned long long)v.tile_cache, (unsigned long long)v.tile_draw,
                        (unsigned long long)v.raster, (unsigned long long)v.composite);
                    std::fclose(progress);
                }
                std::printf("PSP smoke_progress=%llu\n", (unsigned long long)game->frames()); std::fflush(stdout);
            }
            if (smoke && game->frames() >= smoke) {
                frame_progress.active(false, Phase::Shutdown);
                watchdog.audio_thread(-1);
                audio.pause();
                if (audio.stats().failed) throw std::runtime_error("PSP audio shutdown/drain failed");
                report_smoke("ok", game.get(), audio, watchdog, now_us() - boot_time); exit_reason.store(3); running.store(0);
            }
        } catch (const std::exception& error) {
            frame_progress.active(false, Phase::Idle);
            watchdog.stop();
            checkpoint("caught_exception_before_audio_close");
            checkpoint(error.what()); // Preserve the reason before a potentially blocked drain.
            watchdog.audio_thread(-1);
            audio.close();
            const bool oom = dynamic_cast<const std::bad_alloc*>(&error) != nullptr;
            std::snprintf(message, sizeof(message), "%s", oom ? "Out of PSP-1000 memory. No game is running." : error.what());
            FILE* fault = std::fopen("psp-fault.log", "a");
            if (fault) { std::fprintf(fault, "%s heap_free=%lu\n", message, (unsigned long)mallinfo().fordblks); std::fclose(fault); }
            std::printf("PSP fault=%s\n", message); std::fflush(stdout);
            if (smoke) { report_smoke(message, game.get(), audio, watchdog, now_us() - boot_time); exit_reason.store(4); running.store(0); }
            game.reset(); menu = true; selection = 0;
        }
    }
    frame_progress.active(false, Phase::Shutdown);
    watchdog.stop();
    checkpoint("shutdown_before_audio_close");
    watchdog.audio_thread(-1);
    audio.close();
    checkpoint("shutdown_audio_closed");
    if (!smoke && !save_cabinet(game.get())) {
        FILE* fault = std::fopen("psp-fault.log", "a");
        if (fault) { std::fputs("Cabinet save failed during shutdown\n", fault); std::fclose(fault); }
    }
    game.reset();
    running.store(0);
    if (callbacks >= 0) { sceKernelWakeupThread(callbacks); sceKernelWaitThreadEnd(callbacks, nullptr); sceKernelDeleteThread(callbacks); }
    sceKernelExitGame();
    return 0;
}
