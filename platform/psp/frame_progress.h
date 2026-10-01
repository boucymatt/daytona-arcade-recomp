// Main publishes only atomics; the observer never touches a live game object.
#pragma once
#include <atomic>
#include <cstdint>

namespace psp {
enum class FramePhase : uint32_t {
    Idle, Loading, Input, GameFrame, AudioSubmit, TextureWait, TextureUpload,
    GuBuild, GuFinish, GuWait, Vblank, Swap, FrameComplete, Paused, Shutdown
};
inline const char* phase_name(FramePhase phase) {
    switch (phase) {
    case FramePhase::Idle: return "idle";
    case FramePhase::Loading: return "loading";
    case FramePhase::Input: return "input";
    case FramePhase::GameFrame: return "game_frame";
    case FramePhase::AudioSubmit: return "audio_submit";
    case FramePhase::TextureWait: return "texture_wait";
    case FramePhase::TextureUpload: return "texture_upload";
    case FramePhase::GuBuild: return "gu_build";
    case FramePhase::GuFinish: return "gu_finish";
    case FramePhase::GuWait: return "gu_wait";
    case FramePhase::Vblank: return "vblank";
    case FramePhase::Swap: return "swap";
    case FramePhase::FrameComplete: return "frame_complete";
    case FramePhase::Paused: return "paused";
    case FramePhase::Shutdown: return "shutdown";
    }
    return "unknown";
}
struct ProgressSnapshot {
    uint32_t event = 0, frames = 0, raster_updates = 0;
    bool active() const { return (event & 0x80u) != 0; }
    FramePhase phase() const { return FramePhase(event & 0x7fu); }
};
class FrameProgress {
public:
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    // Only the main thread calls these methods. Event packs a 24-bit serial,
    // active bit and phase into one coherent, lock-free publication.
    void mark(FramePhase phase) {
        serial_ = (serial_ + 1) & 0xffffffu;
        event_.store((serial_ << 8) | (active_ ? 0x80u : 0) | uint32_t(phase), std::memory_order_release);
    }
    void active(bool value, FramePhase phase) { active_ = value; mark(phase); }
    void reset_frames() { frames_.store(0); raster_updates_.store(0); }
    void completed(uint32_t frames, bool fresh_3d) {
        frames_.store(frames, std::memory_order_relaxed);
        if (fresh_3d) raster_updates_.fetch_add(1, std::memory_order_relaxed);
        mark(FramePhase::FrameComplete);
    }
    ProgressSnapshot snapshot() const {
        return {event_.load(std::memory_order_acquire), frames_.load(std::memory_order_relaxed),
                raster_updates_.load(std::memory_order_relaxed)};
    }
private:
    std::atomic<uint32_t> event_{0}, frames_{0}, raster_updates_{0};
    uint32_t serial_ = 0;
    bool active_ = false;
};

class StallDetector {
public:
    static constexpr uint32_t kTimeoutUs = 5'000'000;
    // Poll substantially more often than the 32-bit microsecond timer wraps.
    // Emit once per incident, re-arm only after progress or inactive state.
    bool observe(ProgressSnapshot current, uint32_t now) {
        if (!current.active()) { tracking_ = reported_ = false; return false; }
        if (!tracking_ || current.event != previous_.event || current.frames != previous_.frames ||
            current.raster_updates != previous_.raster_updates) {
            previous_ = current;
            last_progress_ = now;
            tracking_ = true; reported_ = false;
            return false;
        }
        if (!reported_ && uint32_t(now - last_progress_) >= kTimeoutUs) {
            reported_ = true;
            return true;
        }
        return false;
    }
    uint32_t stalled_us(uint32_t now) const { return uint32_t(now - last_progress_); }
private:
    ProgressSnapshot previous_{};
    uint32_t last_progress_ = 0;
    bool tracking_ = false, reported_ = false;
};
} // namespace psp
