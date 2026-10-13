// Bounded real-ROM bring-up. No interpreter, board test guards, audio or GPU.
// Frame boundaries follow GameLoop::probe, without its sound/iostream owners.
#include "runtime/gen_support.h"
#include "runtime/m2_board.h"
#ifdef PS3_RSX_GAME
#include "../ps3/game_geometry.h"
#include "../ps3/native_audio.h"
extern "C" int ps3_sample_controls(uint8_t *);
#endif
#ifndef PS3_GAME_PROBE
#include "tls_runtime.h"
#else
#include <sysutil/sysutil.h>
#ifdef PS3_PERF_PROBE
#include <sys/systime.h>
#endif
extern "C" int ps3_probe_running(void);
#ifdef PS3_GAME_DISPLAY
extern "C" int ps3_present(const uint32_t *, unsigned, unsigned);
#endif
#endif
#include "game_assets.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#ifdef PS3_PERF_PROBE
extern uint64_t ps3_span_ticks[4], ps3_span_calls[4], ps3_span_pixels[4];
static uint64_t ps3_profile_ticks() {
    uint64_t t; __asm__ volatile("mftb %0" : "=r"(t) :: "memory"); return t;
}
#endif
#ifdef M2_RASTER_DIAGNOSTICS
static FILE *raster_diagnostic_log = nullptr;
bool geometry_trace_active() { return raster_diagnostic_log != nullptr; }
void geometry_clip_diagnostic(const char *stage, const uint32_t *values, unsigned count) {
    if (!raster_diagnostic_log) return;
    fprintf(raster_diagnostic_log, "GeoClip %s", stage);
    for (unsigned i = 0; i < count; ++i) fprintf(raster_diagnostic_log, " %08x", unsigned(values[i]));
    fprintf(raster_diagnostic_log, "\n");
    fflush(raster_diagnostic_log);
}
static uint32_t geometry_values[3][41];
static unsigned geometry_counts[3];
void raster_geometry_diagnostic(unsigned stage, const uint32_t *values, unsigned count) {
    if (!raster_diagnostic_log || stage >= 3 || count > 41) return;
    geometry_counts[stage] = count;
    for (unsigned i = 0; i < count; ++i) geometry_values[stage][i] = values[i];
}
void raster_pixel_diagnostic(int x, int y, const uint32_t *values, unsigned count) {
    if (!raster_diagnostic_log) return;
    for (unsigned stage = 0; stage < 3; ++stage) {
        fprintf(raster_diagnostic_log, "RasterGeometry %d %d %u", x, y, stage);
        for (unsigned i = 0; i < geometry_counts[stage]; ++i)
            fprintf(raster_diagnostic_log, " %08x", unsigned(geometry_values[stage][i]));
        fprintf(raster_diagnostic_log, "\n");
    }
    fprintf(raster_diagnostic_log, "RasterPixel %d %d", x, y);
    for (unsigned i = 0; i < count; ++i) fprintf(raster_diagnostic_log, " %08x", unsigned(values[i]));
    fprintf(raster_diagnostic_log, "\n");
    fflush(raster_diagnostic_log);
}
#endif
#ifdef M2_XBOX360_VIDEO_TRACE
static FILE *video_trace_log = nullptr;
void xbox_video_buffer(const char *name, const uint32_t *data, size_t count, size_t expected) {
    if (!video_trace_log) return;
    fprintf(video_trace_log, "Video buffer %s address=%08x", name, unsigned(reinterpret_cast<uintptr_t>(data)));
    fprintf(video_trace_log, " count=%u expected=%u\n", unsigned(count), unsigned(expected));
    if (fflush(video_trace_log)) throw std::runtime_error("video buffer log write failed");
    if (!data || count != expected) throw std::runtime_error("video buffer invariant failed");
}
void xbox_video_checkpoint(const char *phase) {
    if (!video_trace_log) return;
    fprintf(video_trace_log, "%s\n", phase);
    if (fflush(video_trace_log)) throw std::runtime_error("video checkpoint write failed");
}
#endif
#ifndef XBOX_GAME_HOST_TEST
#include "game_expected.h"
#ifdef XBOX_GAME_PACED_SLICE
#include <xecore/xboxkrnl_threads.h>
#endif
int run_exception_cases();
#endif

#ifdef GAME_PROBE_FRAMES
static constexpr unsigned kFrames = GAME_PROBE_FRAMES;
#elif defined(XBOX_GAME_FOURTH_FRAME)
static constexpr unsigned kFrames = 4;
#elif defined(XBOX_GAME_PACED_STARTUP)
static constexpr unsigned kFrames = 120;
#elif defined(XBOX_GAME_SECOND_FRAME)
static constexpr unsigned kFrames = 2;
#elif defined(XBOX_GAME_TRACE)
static constexpr unsigned kFrames = 1;
#else
static constexpr unsigned kFrames = 120;
#endif
#if !defined(XBOX_GAME_HOST_TEST) && !defined(PS3_PERF_PROBE)
static_assert(sizeof(game_expected) / sizeof(game_expected[0]) >= kFrames,
              "Regenerate the host oracle for the requested frame count");
#endif
static uint64_t byte_hash(uint64_t h, unsigned char b) { return (h ^ b) * 1099511628211ull; }
static void word_hash(uint64_t &h, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) h = byte_hash(h, uint8_t(v >> (i * 8)));
}
static std::vector<uint8_t> load(const std::string &dir, unsigned index, FILE *log) {
    const auto &asset = game_assets[index];
    fprintf(log, "Loading %s (%u bytes)\n", asset.name, asset.size); fflush(log);
#if defined(XBOX_GAME_HOST_TEST) || defined(PS3_GAME_PROBE)
    const std::string path = dir + "/" + asset.name;
#else
    const std::string path = dir + "\\" + asset.name;
#endif
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) throw std::runtime_error("cannot open " + path);
    std::unique_ptr<FILE, decltype(&fclose)> owner(file, fclose);
    std::vector<uint8_t> data(asset.size);
    if (fread(data.data(), 1, data.size(), file) != data.size() || fgetc(file) != EOF || ferror(file))
        throw std::runtime_error("wrong size or read error: " + path);
    uint64_t h = 14695981039346656037ull;
    for (uint8_t b : data) h = byte_hash(h, b);
    if (h != asset.hash) throw std::runtime_error("wrong ROM image: " + path);
    return data;
}

class Runner {
public:
#ifdef PS3_PERF_PROBE
    uint64_t geometry_ticks = 0, video_ticks = 0;
#endif
    bool diagnose(FILE *log, const std::string &path, unsigned frame) {
        // Explicit little-endian words, identical format on host and PS3.
        // Private state dump stays alongside private ROM assets, never in source.
        FILE *dump = frame == 181 ? fopen(path.c_str(), "wb") : nullptr;
        if (frame == 181 && !dump) return false;
        bool ok = true;
        auto emit = [&](uint32_t v) {
            unsigned char bytes[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
            if (dump && fwrite(bytes, 1, 4, dump) != 4) ok = false;
        };
        emit(0x31545344); emit(frame); // DST1
        auto section = [&](const char *name, unsigned count, auto read) {
            uint64_t hash = 14695981039346656037ull;
            emit(count);
            for (unsigned i = 0; i < count; ++i) {
                const uint32_t value = read(i);
                word_hash(hash, value); emit(value);
            }
            fprintf(log, "Component frame=%u %s words=%u hash=%08x%08x\n",
                    frame, name, count, unsigned(hash >> 32), unsigned(hash));
        };
        section("cpu", 5 + unsigned(sizeof(cpu.m_r) / sizeof(cpu.m_r[0])), [&](unsigned i) {
            switch (i) {
            case 0: return uint32_t(ls.count);
            case 1: return uint32_t(ls.count >> 32);
            case 2: return uint32_t(cpu.m_IP);
            case 3: return uint32_t(cpu.m_AC);
            case 4: return uint32_t(cpu.m_PC);
            default: return uint32_t(cpu.m_r[i - 5]);
            }
        });
        section("main_ram", 0x20000 / 4, [&](unsigned i) { return board.read_dword(0x200000 + i * 4); });
        section("work_ram", 0x100000 / 4, [&](unsigned i) { return board.read_dword(0x500000 + i * 4); });
        section("video", unsigned(board.video().screen().size()), [&](unsigned i) { return board.video().screen()[i]; });
        section("tgp_count", 2, [&](unsigned i) { return uint32_t(board.tgp().tgp_instructions() >> (32 * i)); });
        if (dump && fclose(dump)) ok = false;
        return fflush(log) == 0 && ok;
    }
    FILE *trace = nullptr;
    bool verbose_trace = false;
    void checkpoint(const char *phase) {
#if defined(XBOX_GAME_QUIET_SLICE) && !defined(XBOX_GAME_FOURTH_FRAME)
        (void)phase;
        return; // Keep trace-driven slicing, but perform no checkpoint I/O.
#else
#ifdef XBOX_GAME_FOURTH_FRAME
        if (!verbose_trace) return;
#endif
        if (!trace) return;
        // Newlib's Xbox 64-bit variadic formatting does not agree with this
        // compiler ABI. Pass only explicitly narrowed 32-bit words.
        fprintf(trace, "%s count=0x%08x%08x", phase, unsigned(ls.count >> 32), unsigned(ls.count));
        fprintf(trace, " ip=%08x\n", unsigned(cpu.m_IP));
        if (fflush(trace)) throw std::runtime_error("checkpoint log write failed");
#endif
    }
    rt::M2Board board;
    rt::Cpu cpu;
    rt::Lockstep ls;
    gen::Env env;
    bool in_vblank = false, done = false;
    uint64_t start = 0, vb_start = 0;
    uint64_t trace_start = 0;
    explicit Runner(rt::M2Board::Images images) : board(std::move(images)), cpu(&board), ls(cpu), env{cpu, ls} {
        board.attach(cpu, ls);
        cpu.reset();
        ls.add_callback(1024, [this] { probe(); });
    }
    void probe() {
#ifdef PS3_GAME_PROBE
        sysUtilCheckCallback();
        if (!ps3_probe_running()) throw std::runtime_error("cancelled from XMB");
#endif
        const bool idle = board.in_idle_loop();
        if (in_vblank) {
            if ((idle && ls.count - vb_start >= 2048) || ls.count - vb_start >= 40000) {
                checkpoint("Before vblank_end");
#ifdef PS3_PERF_PROBE
                const uint64_t video_start = ps3_profile_ticks();
#endif
                board.vblank_end();
#ifdef PS3_PERF_PROBE
                video_ticks += ps3_profile_ticks() - video_start;
#endif
                checkpoint("After vblank_end");
                in_vblank = false; start = ls.count; done = true; ls.end_count = ls.count;
            }
        } else if ((idle && ls.count - start >= 2048) || ls.count - start >= 110000) {
            checkpoint("Before vblank_start");
#ifdef PS3_PERF_PROBE
            const uint64_t geometry_start = ps3_profile_ticks();
#endif
            board.vblank_start(); in_vblank = true; vb_start = ls.count;
#ifdef PS3_PERF_PROBE
            geometry_ticks += ps3_profile_ticks() - geometry_start;
#endif
            checkpoint("After vblank_start");
        }
        ls.add_callback(ls.count + 1024, [this] { probe(); });
    }
    uint64_t frame() {
#ifdef PS3_PERF_PROBE
        geometry_ticks = video_ticks = 0;
#endif
        done = false;
        const uint64_t limit = ls.count + 200000;
        ls.end_count = limit;
        while (!done) {
            if (ls.count >= limit) throw std::runtime_error("frame instruction limit");
            if (trace) {
                ls.end_count = std::min(limit, ls.count + (ls.count - trace_start < 64 ? 1 : 1024));
                checkpoint("Before has_code");
            }
            if (!gen::has_code(cpu.m_IP)) throw std::runtime_error("missing generated i960 code");
            const auto before = ls.count;
            const auto ip = cpu.m_IP;
            checkpoint("Before generated dispatch");
            gen::run(env);
#if defined(XBOX_GAME_PACED_SLICE) && !defined(XBOX_GAME_HOST_TEST)
            if (trace) {
                int64_t interval = -10000; // 1 ms; diagnostic pacing, not a game scheduler.
                if (KeDelayExecutionThread(0, 0, &interval) < 0)
                    throw std::runtime_error("slice delay failed");
            }
#endif
            checkpoint("After generated dispatch");
            if (before == ls.count && ip == cpu.m_IP && !done)
                throw std::runtime_error("generated dispatch made no progress");
        }
#ifdef PS3_PERF_PROBE
        return 0; // Performance mode: no register/RAM/framebuffer hash traversal.
#else
        checkpoint("Before frame signature");
        // Fixed byte order: architectural registers, plain RAM, actual screen.
        uint64_t h = 14695981039346656037ull;
        word_hash(h, uint32_t(ls.count)); word_hash(h, uint32_t(ls.count >> 32));
        word_hash(h, cpu.m_IP); word_hash(h, cpu.m_AC); word_hash(h, cpu.m_PC);
        for (uint32_t r : cpu.m_r) word_hash(h, r);
        checkpoint("Signature registers complete; before main RAM");
        for (uint32_t a = 0x200000; a < 0x220000; a += 4) word_hash(h, board.read_dword(a));
        checkpoint("Signature main RAM complete; before work RAM");
        for (uint32_t a = 0x500000; a < 0x600000; a += 4) word_hash(h, board.read_dword(a));
        checkpoint("Signature work RAM complete; before screen");
        for (uint32_t p : board.video().screen()) word_hash(h, p);
        checkpoint("Signature screen complete; before TGP count");
        word_hash(h, uint32_t(board.tgp().tgp_instructions()));
        word_hash(h, uint32_t(board.tgp().tgp_instructions() >> 32));
        checkpoint("Signature complete");
        return h;
#endif
    }
};

static bool run(const std::string &dir, FILE *log, FILE *expected) {
    rt::M2Board::Images images;
    images.program = load(dir, 0, log); images.main_data = load(dir, 1, log);
    images.copro_tables = load(dir, 2, log); images.copro_data = load(dir, 3, log);
    images.polygons = load(dir, 4, log); images.textures = load(dir, 5, log);
    fprintf(log, "Constructing real board and generated execution\n"); fflush(log);
    auto runner = std::make_unique<Runner>(std::move(images));
#ifdef XBOX_GAME_TRACE
    runner->trace = log;
#endif
    if (expected) fprintf(expected, "static constexpr unsigned long long game_expected[] = {\n");
#ifdef PS3_PERF_PROBE
    const uint64_t frequency = sysGetTimebaseFrequency();
    if (!frequency) throw std::runtime_error("invalid timebase frequency");
    auto ticks = ps3_profile_ticks;
    runner->board.video().set_profile_clock(ps3_profile_ticks);
#ifdef PS3_RSX_GAME
    runner->board.video().set_external_3d(true, true);
    Ps3NativeAudio audio(dir, log);
    fprintf(log, "RSX geometry mode: GPU tiles; approximate lighting; native threaded audio\n");
#endif
    uint64_t game_ticks = 0, display_ticks = 0, max_game = 0, max_display = 0;
    uint64_t geometry = 0, video_total = 0, tile_cache = 0, tile_draw = 0, raster = 0, composite = 0;
    unsigned raster_frames = 0, layer_frames = 0;
    unsigned count = 0;
    uint64_t window_start = ticks();
    fprintf(log, "PERF: %u frames, no state/pixel comparisons; timings in microseconds; display includes flip wait\n", kFrames);
    fflush(log);
    for (unsigned f = 0; f < kFrames; ++f) {
#ifdef PS3_RSX_GAME
        uint8_t controls[6];
        if (!ps3_sample_controls(controls)) {
            fprintf(log, "Normal exit requested by user\n");
            break;
        }
        runner->board.io().inputs = {controls[0], controls[1], controls[2], controls[3], controls[4], controls[5]};
#endif
        const uint64_t start = ticks();
        runner->frame();
#ifdef PS3_RSX_GAME
        audio.send(runner->board.take_sound_bytes());
#endif
        const uint64_t rendered = ticks();
        const auto &video = runner->board.video();
        const auto &profile = video.last_profile();
        geometry += runner->geometry_ticks; video_total += runner->video_ticks;
        tile_cache += profile.tile_cache; tile_draw += profile.tile_draw;
        raster += profile.raster; composite += profile.composite;
        raster_frames += profile.raster != 0;
        layer_frames += profile.layers_rebuilt;
#ifdef PS3_RSX_GAME
        if (!ps3_draw_game_geometry(video))
#else
        if (!ps3_present(video.screen().data(), unsigned(video.width()), unsigned(video.screen().size() / video.width())))
#endif
            throw std::runtime_error("display stopped or flip failed");
        const uint64_t presented = ticks();
        game_ticks += rendered - start;
        display_ticks += presented - rendered;
        max_game = std::max(max_game, rendered - start);
        max_display = std::max(max_display, presented - rendered);
        ++count;
        if (count == 60 || f + 1 == kFrames) {
            auto micros = [&](uint64_t t) { return unsigned(t * 1000000 / frequency); };
            fprintf(log, "PERF frames=%u-%u game_avg_us=%u display_avg_us=%u game_max_us=%u display_max_us=%u elapsed_ms=%u\n",
                    f + 1 - count, f, micros(game_ticks / count), micros(display_ticks / count),
                    micros(max_game), micros(max_display), micros(presented - window_start) / 1000);
            const uint64_t accounted = geometry + video_total;
            fprintf(log, "BREAKDOWN frames=%u-%u execution_other_avg_us=%u geometry_vblank_avg_us=%u video_total_avg_us=%u tile_cache_avg_us=%u tile_draw_avg_us=%u raster_avg_us=%u composite_avg_us=%u raster_frames=%u layer_frames=%u\n",
                    f + 1 - count, f, micros((game_ticks > accounted ? game_ticks - accounted : 0) / count),
                    micros(geometry / count), micros(video_total / count), micros(tile_cache / count),
                    micros(tile_draw / count), micros(raster / count), micros(composite / count), raster_frames, layer_frames);
            if (fflush(log)) return false;
            for (unsigned kind = 0; kind < 4; ++kind) {
                fprintf(log, "SPANS kind=%u avg_us=%u calls=%llu candidate_pixels=%llu\n", kind,
                        micros(ps3_span_ticks[kind] / count),
                        (unsigned long long)ps3_span_calls[kind], (unsigned long long)ps3_span_pixels[kind]);
                ps3_span_ticks[kind] = ps3_span_calls[kind] = ps3_span_pixels[kind] = 0;
            }
            if (fflush(log)) return false;
            game_ticks = display_ticks = max_game = max_display = 0;
            geometry = video_total = tile_cache = tile_draw = raster = composite = 0;
            raster_frames = layer_frames = 0;
            count = 0;
            window_start = ticks();
        }
    }
#else
    for (unsigned f = 0; f < kFrames; ++f) {
#if defined(XBOX_GAME_SECOND_FRAME) || defined(XBOX_GAME_PACED_STARTUP)
        runner->trace = f >= 1 ? log : nullptr;
        runner->trace_start = runner->ls.count;
#endif
#ifdef XBOX_GAME_FOURTH_FRAME
        runner->verbose_trace = f == 3;
#ifdef M2_XBOX360_VIDEO_TRACE
        video_trace_log = f == 3 ? log : nullptr;
#endif
#endif
        fprintf(log, "Begin frame %u ip=%08x\n", f, runner->cpu.m_IP); fflush(log);
#ifdef M2_RASTER_DIAGNOSTICS
        raster_diagnostic_log = f == 181 ? log : nullptr;
#endif
        uint64_t h = runner->frame();
#ifdef M2_RASTER_DIAGNOSTICS
        raster_diagnostic_log = nullptr;
#endif
        runner->checkpoint("Before result comparison");
        bool match = true;
#ifndef XBOX_GAME_HOST_TEST
        match = h == game_expected[f];
        if (!match) fprintf(log, "Expected hash=%08x%08x\n",
                            unsigned(game_expected[f] >> 32), unsigned(game_expected[f]));
#endif
        runner->checkpoint("Before result formatting");
        fprintf(log, "Frame %u instructions=0x%08x%08x", f,
                unsigned(runner->ls.count >> 32), unsigned(runner->ls.count));
        const uint64_t tgp = runner->board.tgp().tgp_instructions();
        fprintf(log, " tgp=0x%08x%08x ip=%08x", unsigned(tgp >> 32), unsigned(tgp), unsigned(runner->cpu.m_IP));
        fprintf(log, " hash=%08x%08x %s\n", unsigned(h >> 32), unsigned(h), match ? "PASS" : "FAIL");
#if defined(PS3_GAME_PROBE) || defined(XBOX_GAME_HOST_TEST)
        if ((f >= 178 && f <= 181) || !match) {
            if (!runner->diagnose(log, dir + "/../frame181-state.bin", f)) {
                fprintf(log, "FAIL: component diagnostics write\n"); fflush(log);
                return false;
            }
        }
#endif
        if (fflush(log) || !match) return false;
#ifdef PS3_GAME_DISPLAY
        const auto &video = runner->board.video();
        if (!ps3_present(video.screen().data(), unsigned(video.width()), unsigned(video.screen().size() / video.width())))
            throw std::runtime_error("display stopped or flip failed");
#endif
        runner->checkpoint("Result written");
        if (expected) fprintf(expected, "0x%08x%08xull,\n", unsigned(h >> 32), unsigned(h));
    }
#endif
    if (expected) fprintf(expected, "};\n");
    runner->checkpoint("Before runner destruction");
    runner.reset();
#ifdef M2_XBOX360_VIDEO_TRACE
    video_trace_log = nullptr;
#endif
    fprintf(log, "Runner destroyed\n"); fflush(log);
    return true;
}

#ifdef PS3_GAME_PROBE
extern "C" int ps3_game_probe(FILE *log) {
    if (!log) return 1;
#ifdef PS3_PERF_PROBE
    fprintf(log, "PS3 performance run: %u startup frames, hashes disabled\n", kFrames);
#else
    fprintf(log, "PS3 real runtime: %u startup frames, host hash comparison; no audio\n", kFrames);
#endif
    fflush(log);
    bool ok = false;
    try { ok = run("/dev_hdd0/game/DAYTPS301/USRDIR/roms", log, nullptr); }
    catch (const std::exception &e) {
        if (!ps3_probe_running()) { fprintf(log, "Normal exit requested by user\n"); ok = true; }
        else fprintf(log, "FAIL: %s\n", e.what());
    }
    catch (...) { fprintf(log, "FAIL: unknown exception\n"); }
#ifdef PS3_PERF_PROBE
    fprintf(log, "PS3 performance run: %s (not a parity test)\n", ok ? "COMPLETE" : "FAILED");
#else
    fprintf(log, "PS3 game probe complete: %s\n", ok ? "PASS" : "FAIL");
#endif
    fflush(log);
    return ok ? 0 : 1;
}
#else
int main(int argc, char **argv) {
#ifdef XBOX_GAME_HOST_TEST
    if (argc != 3) return 2;
    FILE *log = stderr;
    FILE *expected = fopen(argv[2], "wb");
    if (!expected) return 2;
    const std::string dir = argv[1];
    bool ok = true;
#else
    (void)argc; (void)argv;
    FILE *log = fopen(
#ifdef XBOX_GAME_TRACE
        "game:\\xbox360-game-trace.log",
#else
        "game:\\xbox360-game.log",
#endif
        "wb");
    if (!log) return 1;
    FILE *expected = nullptr;
    const std::string dir = "game:\\roms";
    bool ok = xbox_tls_init() != 0;
#endif
    fprintf(log, "Real game probe revision 2 (32-bit log arguments): daytona93, %u frames, no GPU/audio/input/link\n", kFrames); fflush(log);
#ifdef M2_XBOX360_VIDEO_TRACE
    fprintf(log, "Mode: CPU video phase checkpoints enabled; buffer checks v2\n"); fflush(log);
#endif
#ifdef M2_XBOX360_SMALL_DRAW_STACK
    fprintf(log, "Mode: tile draw mask on heap; small-stack comparison\n"); fflush(log);
#endif
#ifdef XBOX_GAME_QUIET_SLICE
#ifdef XBOX_GAME_PACED_STARTUP
    fprintf(log, "Mode: quiet startup slicing; checkpoint writes disabled\n"); fflush(log);
#else
    fprintf(log, "Mode: quiet second-frame slicing; checkpoint writes disabled\n"); fflush(log);
#endif
#endif
#ifdef XBOX_GAME_PACED_SLICE
    fprintf(log, "Mode: paced slices (1 ms); 30-second exit observation\n"); fflush(log);
#endif
#ifdef XBOX_GAME_PACED_STARTUP
#ifdef XBOX_GAME_FOURTH_FRAME
    fprintf(log, "Mode: four frames; detailed frame 3 trace; automatic exit disabled\n"); fflush(log);
#else
    fprintf(log, "Mode: 120 frames; automatic exit disabled; leave through Guide\n"); fflush(log);
#endif
#endif
    try {
#ifndef XBOX_GAME_HOST_TEST
        if (ok && run_exception_cases()) throw std::runtime_error("exception regression");
#endif
        if (ok) ok = run(dir, log, expected);
    } catch (const std::exception &e) { fprintf(log, "FAIL: %s\n", e.what()); ok = false; }
    catch (...) { fprintf(log, "FAIL: unknown exception\n"); ok = false; }
#ifdef XBOX_GAME_HOST_TEST
    ok &= fclose(expected) == 0;
#else
    fprintf(log, "Before TLS cleanup\n"); fflush(log);
    xbox_tls_thread_cleanup(); ok &= xbox_tls_shutdown() != 0;
    fprintf(log, "After TLS cleanup\n"); fflush(log);
#endif
    fprintf(log, "Game probe complete: %s\n", ok ? "PASS" : "FAIL");
    ok &= fflush(log) == 0;
#ifndef XBOX_GAME_HOST_TEST
#ifdef XBOX_GAME_PACED_SLICE
    // Keep main alive after teardown: distinguish execution from dashboard return.
    for (unsigned second = 0; second < 30; ++second) {
        fprintf(log, "Exit observation %u/30: main still alive\n", second + 1);
        if (fflush(log)) { ok = false; break; }
        int64_t interval = -10000000;
        if (KeDelayExecutionThread(0, 0, &interval) < 0) { ok = false; break; }
    }
#ifdef XBOX_GAME_PACED_STARTUP
    fprintf(log, "Parking without CRT exit; closing log; leave through Guide\n");
#else
    fprintf(log, "Returning from main; CRT will request dashboard launch\n");
#endif
    ok &= fflush(log) == 0;
#endif
    ok &= fclose(log) == 0;
#ifdef XBOX_GAME_PACED_STARTUP
    // Do not trigger the suspect automatic dashboard-return path. No more I/O.
    for (;;) {
        int64_t interval = -10000000;
        KeDelayExecutionThread(0, 0, &interval);
    }
#endif
#endif
    return ok ? 0 : 1;
}
#endif
