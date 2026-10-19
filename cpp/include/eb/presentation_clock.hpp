#pragma once
#include "eb/frame_pacer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace eb {
// Independent presentation deadlines. Zero draw rate means uncapped; it never
// removes the simulation deadline. All times are injectable for deterministic tests.
class PresentationClock {
public:
    using Time = FramePacer::Time;
    using Duration = FramePacer::Clock::duration;
    PresentationClock(Time now, double rate, bool guard_enabled = true) : guard_enabled_(guard_enabled) {
        reset(now, rate);
    }
    // Changing the guard never rebases simulation, presentation or motion time.
    void set_guard_enabled(bool enabled) { guard_enabled_ = enabled; }
    void reset(Time now, double rate) {
        tick_ = draw_ = now;
        draw_period_ = rate > 0 ? std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1 / rate)) : Duration::zero();
        presentation_cost_ = Duration::zero();
    }
    void resume(Time now) {
        if (now - tick_ > std::chrono::milliseconds(250)) {
            tick_ = draw_ = now;
            presentation_cost_ = Duration::zero();
        }
    }
    bool simulation_due(Time now) const { return now >= tick_; }
    void simulated(std::uint64_t frames) { tick_ += FramePacer::period() * frames; }
    bool presentation_due(Time now) const {
        return fits_before_tick(now) && (draw_period_ == Duration::zero() || now >= draw_);
    }
    // Measure all presentation work, not just the swap. React immediately to a
    // higher cost, then decay slowly; a long stall cannot grow an unbounded guard.
    void presented(Time now, Duration cost = Duration::zero()) {
        if (cost >= Duration::zero()) {
            cost = std::min(cost, FramePacer::period() / 4);
            presentation_cost_ = cost > presentation_cost_ ? cost
                : presentation_cost_ - (presentation_cost_ - cost) / 8;
        }
        if (draw_period_ == Duration::zero()) { draw_ = now; return; }
        draw_ += draw_period_;
        // Omit missed presentation slots, never burst them or drop game ticks.
        if (draw_ <= now) draw_ += draw_period_ * ((now - draw_) / draw_period_ + 1);
    }
    double fraction(Time now) const {
        // Keep the source timeline independent of CPU/render cost. Moving its
        // phase can promote endpoints before their blend begins, causing a jump
        // followed by a hold. Truly overdue source frames still clamp safely.
        return std::clamp(1.0 + double((now - tick_).count()) /
                                    double(FramePacer::period().count()),
                          0.0, 1.0);
    }
    Time wake(Time now) const {
        if (simulation_due(now)) return now;
        const auto slot = draw_period_ == Duration::zero() ? now : std::max(now, draw_);
        return fits_before_tick(slot) ? slot : tick_;
    }
private:
    Duration clearance() const {
        // Small proportional headroom covers ordinary variance without a fixed
        // millisecond tax. Bound it so expensive draws cannot starve presentation.
        return guard_enabled_ ? std::min(presentation_cost_ + presentation_cost_ / 8, FramePacer::period() / 4)
                              : Duration::zero();
    }
    bool fits_before_tick(Time now) const {
        return now < tick_ && clearance() <= tick_ - now;
    }
    Time tick_{}, draw_{};
    Duration draw_period_{};
    Duration presentation_cost_{};
    bool guard_enabled_ = true;
};
} // namespace eb
