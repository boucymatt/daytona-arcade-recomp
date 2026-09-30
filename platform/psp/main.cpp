#include "controls.h"
#include "native_audio.h"
#include "rom_loader.h"

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <psppower.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>
#include <malloc.h>
#include <memory>
#include <new>
#include <stdexcept>

PSP_MODULE_INFO("Daytona Recomp PSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_MAIN_THREAD_PRIORITY(0x20);
PSP_MAIN_THREAD_STACK_SIZE_KB(256);
PSP_HEAP_SIZE_KB(-1);
PSP_HEAP_THRESHOLD_SIZE_KB(1024);

namespace {
std::atomic<uint32_t> running{1};
alignas(64) unsigned int display_list[4096];
constexpr unsigned kStride = 512, kDisplayBytes = kStride * 272 * 2;
constexpr unsigned kTextureOffset = 2 * kDisplayBytes, kTextureBytes = 512 * 512 * 2;
static_assert(kTextureOffset + kTextureBytes <= 2 * 1024 * 1024);
static_assert(uint32_t(psp::Cross) == PSP_CTRL_CROSS && uint32_t(psp::Select) == PSP_CTRL_SELECT && uint32_t(psp::R) == PSP_CTRL_RTRIGGER);
static_assert(psp::argb_to_565(0xffff0000) == 31 && psp::argb_to_565(0xff0000ff) == 0xf800);

uint64_t now_us() { return uint64_t(sceKernelGetSystemTimeWide()); }
int exit_callback(int, int, void*) { running.store(0); return 0; }
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
    void present(const std::vector<uint32_t>& screen, bool stretch) {
        if (screen.size() != 496u * 384u) throw std::runtime_error("Invalid CPU framebuffer size");
        // The prior draw is complete before this sole texture is modified.
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        auto* pixels = reinterpret_cast<uint16_t*>(uncached_ + kTextureOffset);
        for (unsigned y = 0; y < 384; ++y) {
            auto* row = pixels + y * 512;
            for (unsigned x = 0; x < 496; ++x) row[x] = psp::argb_to_565(screen[y * 496 + x]);
            std::fill(row + 496, row + 512, row[495]);
        }
        std::memcpy(pixels + 384 * 512, pixels + 383 * 512, 512 * sizeof(uint16_t));
        sceGuStart(GU_DIRECT, display_list);
        sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT);
        sceGuEnable(GU_TEXTURE_2D); sceGuTexMode(GU_PSM_5650, 0, 0, 0);
        sceGuTexImage(0, 512, 512, 512, vram_ + kTextureOffset);
        sceGuTexFlush(); sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
        sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        const auto rect = psp::viewport(stretch);
        struct Vertex { float u, v, x, y, z; };
        // Narrow strips fit the GE texture cache and retain the same mapping.
        for (unsigned left = 0; left < 496; left += 32) {
            const unsigned right = std::min(left + 32, 496u);
            auto* vertices = static_cast<Vertex*>(sceGuGetMemory(2 * sizeof(Vertex)));
            vertices[0] = {float(left), 0, rect.x + float(left) * rect.width / 496.f, 0, 0};
            vertices[1] = {float(right), 384, rect.x + float(right) * rect.width / 496.f, 272, 0};
            sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, nullptr, vertices);
        }
        sceGuDisable(GU_TEXTURE_2D); finish(); swap();
    }
private:
    uint8_t* vram_ = nullptr;
    uint8_t* uncached_ = nullptr;
    uintptr_t draw_offset_ = 0;
    void finish() { sceGuFinish(); sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE); }
    void swap() { sceDisplayWaitVblankStart(); draw_offset_ = reinterpret_cast<uintptr_t>(sceGuSwapBuffers()); }
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
    const auto heap = mallinfo();
    std::printf("PSP stage=%s heap_used=%lu heap_free=%lu system_free=%lu\n", message,
                (unsigned long)heap.uordblks, (unsigned long)heap.fordblks,
                (unsigned long)sceKernelTotalFreeMemSize()); std::fflush(stdout);
    display.text_screen(); pspDebugScreenPrintf("  DAYTONA RECOMP - PSP\n\n  %s\n\n", message);
    pspDebugScreenPrintf("  Heap arena used/free: %lu/%lu KiB\n", (unsigned long)heap.uordblks / 1024,
                        (unsigned long)heap.fordblks / 1024);
    display.present_text();
}
unsigned smoke_frames(unsigned& skip) {
    FILE* file = std::fopen("smoke.txt", "r"); if (!file) return 0;
    unsigned frames = 120; skip = 0;
    std::fscanf(file, "%u %u", &frames, &skip); std::fclose(file);
    skip = std::min(skip, 3u); return std::clamp(frames, 1u, 6000u);
}
using Audio = psp::NativeAudio<snd::NativeSoundEngine>;
void report_smoke(const char* result, rt::GameLoop* game, const Audio& audio, uint64_t elapsed) {
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
        std::fprintf(file, "audio_render_us=%lu\naudio_peak_render_us=%lu\naudio_late_blocks=%lu\n",
            (unsigned long)stats.render_us, (unsigned long)stats.peak_render_us, (unsigned long)stats.late_blocks);
        std::fclose(file);
    }
    std::printf("PSP smoke=%s frames=%llu screen_hash=%016llx audio_failed=%lu\n", result,
                (unsigned long long)(game ? game->frames() : 0), (unsigned long long)hash,
                (unsigned long)stats.failed); std::fflush(stdout);
}
} // namespace

int main() {
    int callbacks = sceKernelCreateThread("daytona_callbacks", callback_thread, 0x11, 4096, PSP_THREAD_ATTR_USER, nullptr);
    if (callbacks >= 0 && sceKernelStartThread(callbacks, 0, nullptr) < 0) {
        sceKernelDeleteThread(callbacks); callbacks = -1;
    }
    sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    // Official PSP maximum, not an overclock. Audio runs at its device clock.
    scePowerSetClockFrequency(333, 333, 166);
    Display display;
    stage(display, "PSP-1000 / 32 MB initialization");
    Settings settings; settings.load();
    unsigned smoke_skip = 0; const unsigned smoke = smoke_frames(smoke_skip);
    if (smoke) settings.display_skip = int(smoke_skip);
    psp::Controls controls;
    std::unique_ptr<rt::GameLoop> game;
    Audio audio;
    bool menu = true, start_requested = smoke != 0;
    int selection = 0;
    uint32_t held = 0;
    char message[192] = "Start loads your imported files from roms/.";
    const uint64_t boot_time = now_us();
    uint64_t deadline = boot_time;
    uint32_t board_us = 0, present_us = 0;
    constexpr uint64_t frame_us = 17384; // 656 * 424 / 16 MHz, rounded for host pacing only.
    auto apply_settings = [&] { audio.volume(unsigned(settings.volume)); audio.mute(settings.mute != 0); };
    auto load_game = [&] {
        audio.close();
        if (!smoke && !save_cabinet(game.get())) throw std::runtime_error("Cabinet save failed before reset");
        game.reset(); controls = {};
        stage(display, "Loading checked ROM regions (paged)");
        auto loaded = psp::load_game("roms");
        stage(display, "Creating native main board and CPU renderer");
        game = std::make_unique<rt::GameLoop>(std::move(loaded.images), false);
        if (!smoke) {
            load_nv("ioboard_eeprom.bin", game->board().io().eeprom);
            load_nv("backup_ram.bin", game->board().backup_ram());
        }
        if (!audio.open(std::move(loaded.audio))) throw std::runtime_error("Native audio engine missing");
        apply_settings(); stage(display, "Starting dedicated 48 kHz audio thread");
        if (!audio.resume()) throw std::runtime_error("PSP SRC audio/thread startup failed");
        stage(display, "Ready: CPU framebuffer + GU presentation");
        controls.latch(held); deadline = now_us(); menu = false;
    };
    while (running.load()) {
        SceCtrlData raw{}; sceCtrlPeekBufferPositive(&raw, 1);
        const uint32_t pressed = raw.Buttons & ~held; held = raw.Buttons;
        try {
            if (start_requested) { start_requested = false; load_game(); }
            if (!menu && psp::menu_chord(held)) {
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
                        else { if (!audio.resume()) throw std::runtime_error("Audio resume failed"); menu = false; controls.latch(held); deadline = now_us(); }
                    } else if (selection == 2 || selection == 3) {
                        if (selection == 2) settings.mute ^= 1; else settings.stretch ^= 1;
                        apply_settings(); std::snprintf(message, sizeof(message), "%s", settings.save() ? "Options saved." : "Could not save options.");
                    } else if (selection == 5) load_game();
                    else if (selection == 6) std::snprintf(message, sizeof(message), "%s", settings.save() ? "Options saved." : "Could not save options.");
                    else if (selection == 7) running.store(0);
                }
                if (!menu) continue;
                display.text_screen();
                pspDebugScreenPrintf("  DAYTONA RECOMP - PSP-1000\n  CPU renderer / native audio (experimental)\n\n");
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
                display.present_text(); continue;
            }
            const uint64_t now = now_us();
            if (!smoke && now < deadline) { sceKernelDelayThread(unsigned(std::min<uint64_t>(deadline - now, 2000))); continue; }
            const auto mapped = controls.sample({held, raw.Lx});
            rt::Inputs inputs;
            if (!smoke) {
                inputs.steer = mapped.steer; inputs.accel = mapped.accel; inputs.brake = mapped.brake;
                inputs.in0 = mapped.in0; inputs.in1 = mapped.in1; inputs.in2 = mapped.in2;
            }
            scePowerTick(0);
            const auto board_begin = smoke ? now_us() : 0;
            game->run_frame(inputs);
            if (smoke) board_us = uint32_t(now_us() - board_begin);
            const auto bytes = game->board().take_sound_bytes();
            if (!audio.send(bytes.data(), bytes.size()) || audio.stats().failed)
                throw std::runtime_error("Native audio error or command queue overflow");
            present_us = 0;
            if ((game->frames() - 1) % unsigned(settings.display_skip + 1) == 0 || (smoke && game->frames() == smoke)) {
                const auto present_begin = smoke ? now_us() : 0;
                display.present(game->screen(), settings.stretch != 0);
                if (smoke) present_us = uint32_t(now_us() - present_begin);
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
                    std::fclose(progress);
                }
                std::printf("PSP smoke_progress=%llu\n", (unsigned long long)game->frames()); std::fflush(stdout);
            }
            if (smoke && game->frames() >= smoke) {
                audio.pause();
                if (audio.stats().failed) throw std::runtime_error("PSP audio shutdown/drain failed");
                report_smoke("ok", game.get(), audio, now_us() - boot_time); running.store(0);
            }
        } catch (const std::exception& error) {
            audio.close();
            const bool oom = dynamic_cast<const std::bad_alloc*>(&error) != nullptr;
            std::snprintf(message, sizeof(message), "%s", oom ? "Out of PSP-1000 memory. No game is running." : error.what());
            FILE* fault = std::fopen("psp-fault.log", "a");
            if (fault) { std::fprintf(fault, "%s heap_free=%lu\n", message, (unsigned long)mallinfo().fordblks); std::fclose(fault); }
            std::printf("PSP fault=%s\n", message); std::fflush(stdout);
            if (smoke) { report_smoke(message, game.get(), audio, now_us() - boot_time); running.store(0); }
            game.reset(); menu = true; selection = 0;
        }
    }
    audio.close();
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
