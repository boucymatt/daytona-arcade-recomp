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
    ++outputs; std::this_thread::sleep_for(std::chrono::milliseconds(1)); return 512;
}
int sceKernelCreateThread(const char*, int (*entry)(SceSize, void*), int priority, int stack, unsigned, void*) {
    CHECK(priority == 0x12 && stack >= 128 * 1024);
    const int id = next_thread++; threads[id].entry = entry; return id;
}
int sceKernelStartThread(int id, SceSize size, void* argument) {
    if (start_failure.load()) return -77;
    CHECK(size == sizeof(void*)); auto& t = threads.at(id); t.argument = *static_cast<void**>(argument);
    t.thread = std::thread([&t, size] { t.entry(size, &t.argument); }); return 0;
}
int sceKernelWaitThreadEnd(int id, unsigned*) { threads.at(id).thread.join(); return 0; }
int sceKernelDeleteThread(int id) { threads.erase(id); return 0; }
int sceKernelDelayThread(unsigned) { std::this_thread::sleep_for(std::chrono::microseconds(100)); return 0; }
void sceKernelDcacheWritebackRange(void*, unsigned) {}
unsigned sceKernelGetSystemTimeLow() { return unsigned(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }

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
    std::puts("PSP native audio: ordered bounded queue,512-frame doublebuffer,pause/mute/faults and SRC lifetime passed");
}
