// Device-clocked native audio, without SDL or reference sound-board execution.
#pragma once
#include <pspaudio.h>
#include <pspkernel.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

namespace psp {
template<class Engine> class NativeAudio {
public:
    static constexpr uint32_t kQueueSize = 16384;
    static constexpr unsigned kFrames = 512;
    struct Stats {
        uint32_t blocks = 0, frames = 0, queued = 0, overflows = 0;
        uint32_t failed = 0, system_error = 0, invalid = 0, unsupported = 0;
        uint32_t notes = 0, voices = 0, render_us = 0, peak_render_us = 0, late_blocks = 0;
    };
    NativeAudio() = default;
    NativeAudio(const NativeAudio&) = delete;
    NativeAudio& operator=(const NativeAudio&) = delete;
    ~NativeAudio() { close(); }

    // Main owns lifecycle; only the worker touches the engine while running.
    bool open(std::unique_ptr<Engine> engine) {
        close();
        if (!engine || reserved_) return false;
        engine_ = std::move(engine);
        read_ = write_ = blocks_ = frames_ = overflows_ = failed_ = system_error_ = 0;
        invalid_ = unsupported_ = notes_ = voices_ = render_us_ = peak_render_us_ = late_blocks_ = 0;
        return true; // Paused until game entry.
    }
    bool resume() {
        if (thread_ >= 0) return !failed_.load(std::memory_order_relaxed);
        if (!engine_ || reserved_ || failed_.load(std::memory_order_relaxed)) return false;
        int result = sceAudioSRCChReserve(kFrames, 48000, 2);
        if (result < 0) { system_error_ = uint32_t(result); return false; }
        reserved_ = true;
        stop_.store(0, std::memory_order_release);
        thread_ = sceKernelCreateThread("daytona_audio", entry, 0x12, 128 * 1024,
                                       PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, nullptr);
        if (thread_ < 0) { system_error_ = uint32_t(thread_); pause(); return false; }
        auto* self = this;
        result = sceKernelStartThread(thread_, sizeof(self), &self);
        if (result < 0) {
            system_error_ = uint32_t(result);
            sceKernelDeleteThread(thread_); thread_ = -1;
            pause(); return false;
        }
        return true;
    }
    void pause() {
        stop_.store(1, std::memory_order_release);
        if (thread_ >= 0) {
            // Never terminate a thread inside a ROM read or mixer call.
            sceKernelWaitThreadEnd(thread_, nullptr);
            sceKernelDeleteThread(thread_); thread_ = -1;
        }
        if (reserved_) {
            // Both fixed output buffers remain owned here throughout release.
            int result = 0;
            for (unsigned retry = 0; retry < 250; ++retry) {
                result = sceAudioSRCChRelease();
                // SRC uses a distinct busy code while queued samples drain;
                // the generic audio-channel busy code is not sufficient.
                constexpr uint32_t kSrcOutputBusy = 0x80268002u;
                if (uint32_t(result) != uint32_t(SCE_AUDIO_ERROR_OUTPUT_BUSY) &&
                    uint32_t(result) != kSrcOutputBusy) break;
                sceKernelDelayThread(1000);
            }
            if (result < 0) { system_error_ = uint32_t(result); failed_ = 1; }
            else reserved_ = false;
        }
    }
    void close() { pause(); engine_.reset(); }
    bool available() const { return engine_ != nullptr; }
    void volume(unsigned percent) { gain_.store(std::min(percent, 100u), std::memory_order_relaxed); }
    void mute(bool value) { muted_.store(value, std::memory_order_relaxed); }
    bool send(const uint8_t* data, size_t count) {
        if (!count) return true;
        if (!engine_ || !data || count > kQueueSize || failed_.load(std::memory_order_relaxed)) return false;
        const uint32_t w = write_.load(std::memory_order_relaxed);
        const uint32_t r = read_.load(std::memory_order_acquire);
        if (count > kQueueSize - uint32_t(w - r)) {
            overflows_.fetch_add(1, std::memory_order_relaxed); return false;
        }
        for (size_t i = 0; i < count; ++i) queue_[(w + uint32_t(i)) & (kQueueSize - 1)] = data[i];
        write_.store(w + uint32_t(count), std::memory_order_release);
        return true;
    }
    Stats stats() const {
        Stats s;
        s.blocks = blocks_.load(); s.frames = frames_.load();
        const auto r = read_.load(std::memory_order_acquire);
        s.queued = write_.load(std::memory_order_acquire) - r;
        s.overflows = overflows_.load(); s.failed = failed_.load(); s.system_error = system_error_.load();
        s.invalid = invalid_.load(); s.unsupported = unsupported_.load();
        s.notes = notes_.load(); s.voices = voices_.load();
        s.render_us = render_us_.load(); s.peak_render_us = peak_render_us_.load(); s.late_blocks = late_blocks_.load();
        return s;
    }
private:
    static_assert(std::atomic<uint32_t>::is_always_lock_free, "PSP audio requires lock-free 32-bit publication");
    std::unique_ptr<Engine> engine_;
    SceUID thread_ = -1;
    bool reserved_ = false;
    std::array<uint8_t, kQueueSize> queue_{};
    // SRC may retain a buffer address after the blocking call. These survive
    // even an unrecoverable release failure and are never freed by close().
    alignas(64) inline static std::array<std::array<int16_t, kFrames * 2>, 2> output_{};
    std::array<float, kFrames * 2> mix_{};
    std::atomic<uint32_t> read_{0}, write_{0}, stop_{1}, gain_{80}, muted_{0};
    std::atomic<uint32_t> blocks_{0}, frames_{0}, overflows_{0}, failed_{0}, system_error_{0};
    std::atomic<uint32_t> invalid_{0}, unsupported_{0}, notes_{0}, voices_{0};
    std::atomic<uint32_t> render_us_{0}, peak_render_us_{0}, late_blocks_{0};

    void receive() {
        uint32_t r = read_.load(std::memory_order_relaxed);
        const uint32_t w = write_.load(std::memory_order_acquire);
        while (r != w) {
            const uint32_t offset = r & (kQueueSize - 1);
            const uint32_t count = std::min(w - r, kQueueSize - offset);
            engine_->send(queue_.data() + offset, count);
            r += count;
        }
        read_.store(r, std::memory_order_release);
    }
    static int entry(SceSize, void* argument) {
        auto* self = *static_cast<NativeAudio**>(argument);
        return self->run();
    }
    int run() noexcept {
        unsigned index = 0;
        while (!stop_.load(std::memory_order_acquire)) {
            auto& out = output_[index];
            const uint32_t begin = sceKernelGetSystemTimeLow();
            try {
                receive();
                engine_->render(mix_.data(), kFrames);
                const float gain = muted_.load(std::memory_order_relaxed) ? 0.f : gain_.load(std::memory_order_relaxed) * .01f;
                for (size_t i = 0; i < out.size(); ++i) {
                    const float sample = std::isfinite(mix_[i]) ? mix_[i] * gain : 0.f;
                    out[i] = int16_t(std::clamp(sample, -1.f, 1.f) * 32767.f);
                }
                const auto s = engine_->stats();
                invalid_.store(uint32_t(s.invalid)); unsupported_.store(uint32_t(s.unsupported));
                notes_.store(uint32_t(s.notes)); voices_.store(uint32_t(s.voices));
                if (s.invalid) { failed_.store(1); break; }
            } catch (...) { failed_.store(1); break; }
            const uint32_t elapsed = sceKernelGetSystemTimeLow() - begin;
            render_us_.store(elapsed);
            peak_render_us_.store(std::max(peak_render_us_.load(), elapsed));
            if (elapsed > 10667) late_blocks_.fetch_add(1);
            sceKernelDcacheWritebackRange(out.data(), unsigned(out.size() * sizeof(int16_t)));
            const int result = sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, out.data());
            if (result < 0) { system_error_.store(uint32_t(result)); failed_.store(1); break; }
            blocks_.fetch_add(1); frames_.fetch_add(kFrames);
            index ^= 1;
        }
        return 0;
    }
};
} // namespace psp
