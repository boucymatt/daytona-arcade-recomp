// Bounded real-ROM bring-up. No interpreter, board test guards, audio or GPU.
// Frame boundaries follow GameLoop::probe, without its sound/iostream owners.
#include "runtime/gen_support.h"
#include "runtime/m2_board.h"
#include "tls_runtime.h"
#include "game_assets.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
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

#ifdef XBOX_GAME_FOURTH_FRAME
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
static uint64_t byte_hash(uint64_t h, unsigned char b) { return (h ^ b) * 1099511628211ull; }
static void word_hash(uint64_t &h, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) h = byte_hash(h, uint8_t(v >> (i * 8)));
}
static std::vector<uint8_t> load(const std::string &dir, unsigned index, FILE *log) {
    const auto &asset = game_assets[index];
    fprintf(log, "Loading %s (%u bytes)\n", asset.name, asset.size); fflush(log);
#ifdef XBOX_GAME_HOST_TEST
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
        const bool idle = board.in_idle_loop();
        if (in_vblank) {
            if ((idle && ls.count - vb_start >= 2048) || ls.count - vb_start >= 40000) {
                checkpoint("Before vblank_end");
                board.vblank_end();
                checkpoint("After vblank_end");
                in_vblank = false; start = ls.count; done = true; ls.end_count = ls.count;
            }
        } else if ((idle && ls.count - start >= 2048) || ls.count - start >= 110000) {
            checkpoint("Before vblank_start");
            board.vblank_start(); in_vblank = true; vb_start = ls.count;
            checkpoint("After vblank_start");
        }
        ls.add_callback(ls.count + 1024, [this] { probe(); });
    }
    uint64_t frame() {
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
        uint64_t h = runner->frame();
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
        if (fflush(log) || !match) return false;
        runner->checkpoint("Result written");
        if (expected) fprintf(expected, "0x%08x%08xull,\n", unsigned(h >> 32), unsigned(h));
    }
    if (expected) fprintf(expected, "};\n");
    runner->checkpoint("Before runner destruction");
    runner.reset();
#ifdef M2_XBOX360_VIDEO_TRACE
    video_trace_log = nullptr;
#endif
    fprintf(log, "Runner destroyed\n"); fflush(log);
    return true;
}

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
