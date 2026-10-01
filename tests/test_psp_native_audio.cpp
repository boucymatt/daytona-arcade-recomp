#include "../platform/psp/native_audio.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
namespace {
struct Thread { int (*entry)(SceSize, void*) = nullptr; void* argument = nullptr; std::thread thread; };
std::map<int, Thread> threads;
int next_thread = 1;
std::atomic<unsigned> outputs{0}, releases{0}, busy_releases{0};
std::atomic<uint32_t> release_error{SCE_AUDIO_ERROR_OUTPUT_BUSY};
std::atomic<bool> reserved{false}, release_stuck{false}, start_failure{false};
std::atomic<bool> immediate_output{false}, synthetic_clock{false};
std::atomic<uint32_t> mock_clock{0}, mock_render_us{0}, mock_output_us{0};
std::atomic<unsigned> worker_delays{0}, wrapped_outputs{0};
std::atomic<uint32_t> last_worker_delay{0};
std::atomic<int> worker_delay_error{0};
thread_local bool audio_worker = false;

std::mutex buffers_mutex;
std::vector<const int16_t*> buffers;
struct Observation {
    std::atomic<unsigned> rendered{0}, bytes{0}, destruction{0}, active{0};
    uint64_t digest = 1469598103934665603ull;
    bool fail = false;
};
struct Engine {
    Observation& out;
    explicit Engine(Observation& value) : out(value) {}
    ~Engine() { CHECK(!out.active.load()); ++out.destruction; }
    void send(const uint8_t* bytes, size_t count) {
        for (size_t i = 0; i < count; ++i) { out.digest ^= bytes[i]; out.digest *= 1099511628211ull; }
        out.bytes.fetch_add(unsigned(count));
    }
    void render(float* data, size_t count) {
        if (out.fail) throw std::runtime_error("synthetic render fault");
        ++out.active;
        for (size_t i = 0; i < count * 2; ++i) data[i] = i & 1 ? -.5f : .5f;
        if (synthetic_clock.load()) mock_clock.fetch_add(mock_render_us.load());
        ++out.rendered; --out.active;

    }
    struct Stats { uint32_t invalid = 0, unsupported = 0, notes = 1, voices = 1; };
    Stats stats() const { return {}; }
};
template<class Predicate> void until(Predicate predicate) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate()) { CHECK(std::chrono::steady_clock::now() < end); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
}
}
int sceAudioSRCChReserve(int frames, int rate, int channels) {
    CHECK(frames == 512 && rate == 48000 && channels == 2);
    return reserved.exchange(true) ? int(SCE_AUDIO_ERROR_OUTPUT_BUSY) : 0;
}
int sceAudioSRCChRelease() {
    ++releases;
    if (release_stuck.load()) return int(release_error.load());
    auto n = busy_releases.load();
    if (n) { busy_releases.store(n - 1); return int(release_error.load()); }
    reserved.store(false); return 0;
}
int sceAudioSRCOutputBlocking(int volume, void* buffer) {
    CHECK(volume == PSP_AUDIO_VOLUME_MAX && uintptr_t(buffer) % 64 == 0);
    { std::lock_guard lock(buffers_mutex); buffers.push_back(static_cast<const int16_t*>(buffer)); }
    ++outputs;
    if (synthetic_clock.load()) {
        const auto elapsed = mock_output_us.load();
        const auto before = mock_clock.fetch_add(elapsed);
        if (uint32_t(before + elapsed) < before) ++wrapped_outputs;
    }
    if (!immediate_output.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return 512;

}
int sceKernelCreateThread(const char*, int (*entry)(SceSize, void*), int priority, int stack, unsigned, void*) {
    CHECK(priority == 0x12 && stack >= 128 * 1024);
    const int id = next_thread++; threads[id].entry = entry; return id;
}
int sceKernelStartThread(int id, SceSize size, void* argument) {
    if (start_failure.load()) return -77;
    CHECK(size == sizeof(void*)); auto& t = threads.at(id); t.argument = *static_cast<void**>(argument);
    t.thread = std::thread([&t, size] { audio_worker = true; t.entry(size, &t.argument); }); return 0;
}
int sceKernelWaitThreadEnd(int id, unsigned*) { threads.at(id).thread.join(); return 0; }
int sceKernelDeleteThread(int id) { threads.erase(id); return 0; }
int sceKernelDelayThread(unsigned delay) {
    if (audio_worker) {
        ++worker_delays; last_worker_delay = delay;
        if (worker_delay_error.load()) return worker_delay_error.load();
        if (synthetic_clock.load()) mock_clock.fetch_add(delay);
    }
    std::this_thread::sleep_for(std::chrono::microseconds(100)); return 0;
}

void sceKernelDcacheWritebackRange(void*, unsigned) {}
unsigned sceKernelGetSystemTimeLow() {
    if (audio_worker && synthetic_clock.load()) return mock_clock.load();
    return unsigned(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

int main() {
    using Audio = psp::NativeAudio<Engine>;
    Observation observation;
    Audio audio; CHECK(audio.open(std::make_unique<Engine>(observation)));
    std::array<uint8_t, Audio::kQueueSize> bytes{};
    uint64_t expected = 1469598103934665603ull;
    for (unsigned i = 0; i < bytes.size(); ++i) {
        bytes[i] = uint8_t(i); expected ^= bytes[i]; expected *= 1099511628211ull;
    }
    CHECK(audio.send(bytes.data(), bytes.size())); CHECK(!audio.send(bytes.data(), 1));
    CHECK(audio.stats().overflows == 1 && audio.stats().queued == bytes.size());
    CHECK(audio.resume()); until([&] { return observation.rendered >= 3; });
    busy_releases.store(3); const auto before = releases.load(); audio.pause();
    CHECK(releases == before + 4 && !reserved && observation.digest == expected);
    CHECK(observation.bytes == bytes.size() && audio.stats().queued == 0);
    CHECK(buffers.size() >= 3 && buffers[0] != buffers[1] && buffers[0] == buffers[2]);
    CHECK(buffers.back()[0] > 13000 && buffers.back()[0] < 13200);
    const auto old = observation.rendered.load(); audio.mute(true); CHECK(audio.resume());
    until([&] { return observation.rendered >= old + 3; });
    // Actual SRC busy result differs from the SDK generic audio busy macro.
    release_error = 0x80268002u; busy_releases = 3;
    const auto src_before = releases.load(); audio.pause();
    CHECK(releases == src_before + 4 && !reserved);
    CHECK(!audio.stats().failed && !audio.stats().system_error);
    CHECK(buffers.back()[0] == 0 && buffers.back()[1] == 0);
    CHECK(observation.destruction == 0); audio.close(); CHECK(observation.destruction == 1);

    // A mixer over its 10.67ms budget plus an empty SRC queue must not create
    // an indefinitely runnable high-priority loop. All bytes/samples remain
    // ordered; a bounded scheduler wait follows every successful output call.
    Observation slow;
    synthetic_clock = true; immediate_output = true;
    mock_clock = 0; mock_render_us = 12000; mock_output_us = 0;
    const auto delays_before = worker_delays.load();
    CHECK(audio.open(std::make_unique<Engine>(slow)) && audio.send(bytes.data(), bytes.size()) && audio.resume());
    until([&] { return audio.stats().fairness_yields >= 5; }); audio.pause();
    auto stats = audio.stats();
    CHECK(!stats.failed && stats.late_blocks >= 5 && stats.render_us == 12000);
    CHECK(stats.output_call_us == 0 && worker_delays >= delays_before + 5 && last_worker_delay == 1000);
    CHECK(stats.frames == stats.blocks * 512 && slow.rendered == stats.blocks &&
          slow.bytes == bytes.size() && slow.digest == expected);
    audio.close(); CHECK(slow.destruction == 1);

    // A partially blocking call gets only the remainder, including when its
    // start/end timestamps straddle the low system timer's wrap boundary.
    Observation rollover;
    mock_clock = 0xffffff80u; mock_render_us = 0; mock_output_us = 250;
    const auto wraps_before = wrapped_outputs.load();
    CHECK(audio.open(std::make_unique<Engine>(rollover)) && audio.resume());
    until([&] { return audio.stats().fairness_yields >= 5; }); audio.pause();
    stats = audio.stats();
    CHECK(!stats.failed && stats.output_call_us == 250 && last_worker_delay == 750);
    CHECK(wrapped_outputs == wraps_before + 1 && stats.frames == stats.blocks * 512);
    audio.close(); CHECK(rollover.destruction == 1);

    // Even a long call duration is not proof that the thread actually slept.
    // Both normally blocking and slow, nonblocking calls get the minimum
    // explicit scheduler wait. No generated samples are shortened or dropped.
    Observation blocking;
    immediate_output = false; mock_clock = 0; mock_output_us = 2500;
    const auto normal_delays = worker_delays.load();
    CHECK(audio.open(std::make_unique<Engine>(blocking)) && audio.resume());
    until([&] { return audio.stats().fairness_yields >= 5; }); audio.pause();
    stats = audio.stats();
    CHECK(!stats.failed && stats.fairness_yields >= 5 && last_worker_delay == 250);
    CHECK(stats.output_call_us == 2500 && worker_delays >= normal_delays + 5);
    audio.close(); CHECK(blocking.destruction == 1);

    Observation long_nonblocking;
    immediate_output = true; mock_clock = 0; mock_output_us = 12000;
    CHECK(audio.open(std::make_unique<Engine>(long_nonblocking)) && audio.resume());
    until([&] { return audio.stats().fairness_yields >= 5; }); audio.pause();
    stats = audio.stats();
    CHECK(!stats.failed && stats.output_call_us == 12000 && last_worker_delay == 250);
    CHECK(stats.frames == stats.blocks * 512 && stats.fairness_yields >= 5);
    audio.close(); CHECK(long_nonblocking.destruction == 1);

    Observation wait_failure;
    immediate_output = true; mock_output_us = 0; worker_delay_error = -88;
    CHECK(audio.open(std::make_unique<Engine>(wait_failure)) && audio.resume());
    until([&] { return audio.stats().failed; }); audio.close();
    CHECK(audio.stats().system_error == uint32_t(-88) && wait_failure.destruction == 1);
    worker_delay_error = 0; synthetic_clock = false; immediate_output = false;

    Observation failure; failure.fail = true;
    CHECK(audio.open(std::make_unique<Engine>(failure)) && audio.resume());
    until([&] { return audio.stats().failed; }); audio.close(); CHECK(failure.destruction == 1);
    Observation start; CHECK(audio.open(std::make_unique<Engine>(start)));
    start_failure = true; CHECK(!audio.resume()); start_failure = false;
    audio.close(); CHECK(start.destruction == 1 && !reserved);

    // A broken SRC release must not leave hardware pointing into freed object storage.
    const int16_t* retained = nullptr;
    Observation stuck;
    {
        Audio scoped; CHECK(scoped.open(std::make_unique<Engine>(stuck)) && scoped.resume());
        until([&] { return stuck.rendered >= 2; }); release_stuck = true;
        const auto stuck_before = releases.load(); scoped.pause();
        CHECK(releases == stuck_before + 250 && reserved);
        CHECK(scoped.stats().failed && scoped.stats().system_error == 0x80268002u);
        retained = buffers.back();
    }
    CHECK(stuck.destruction == 1 && retained[0] > 0);
    release_stuck = false; CHECK(sceAudioSRCChRelease() == 0);
    // Unexpected release errors must not be misreported as temporary drain.
    Observation unknown;
    CHECK(audio.open(std::make_unique<Engine>(unknown)) && audio.resume());
    until([&] { return unknown.rendered >= 2; });
    release_stuck = true; release_error = 0x80260008u;
    const auto unknown_before = releases.load(); audio.pause();
    CHECK(releases == unknown_before + 1 && reserved);
    CHECK(audio.stats().failed && audio.stats().system_error == 0x80260008u);
    release_stuck = false; audio.close();
    CHECK(unknown.destruction == 1 && !reserved && audio.stats().failed);
    CHECK(threads.empty());
    std::puts("PSP native audio: ordered queue,doublebuffer,pause/mute/faults,SRC lifetime and fair-yield/rollover passed");
}
