#pragma once
#include <cstdint>

namespace psp {
// Completed-frame counters are zero-based before the next run_frame call.
// A bounded diagnostic window, not a game scheduling or frame-skip policy.
class TraceWindow {
public:
    static constexpr uint64_t kFirst = 205, kLast = 216;
    enum class Action { None, Enter, Leave };
    Action advance(uint64_t next_frame) {
        if (active_ && next_frame > kLast) {
            active_ = false; return Action::Leave;
        }
        if (!entered_ && next_frame >= kFirst && next_frame <= kLast) {
            entered_ = active_ = true; return Action::Enter;
        }
        return Action::None;
    }
    bool active() const { return active_; }
private:
    bool entered_ = false, active_ = false;
};
} // namespace psp
