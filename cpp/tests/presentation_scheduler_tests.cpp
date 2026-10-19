// Synthetic wall time, real pipeline and moving scenes. Verify motion across
// producer updates as well as scheduler work/caps; no physical-display claim.
#include "eb/presentation_pipeline.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using Time = eb::PresentationPipeline::Time;
using namespace std::chrono_literals;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Result {
    unsigned ticks{}, draws{}, samples{}, holds{};
    double max_tick_lateness_ms{}, max_draw_gap_ms{}, max_motion_error{}, motion_error_sum{};
};

Result run(double cap, std::array<std::chrono::microseconds, 2> update_costs,
           std::chrono::microseconds draw_cost, bool guard_enabled = true) {
    const auto period = eb::FramePacer::period();
    eb::DisplaySettings settings;
    settings.frame_limit = cap == 0 ? 0 : 300;
    settings.experimental_frame_pacing = guard_enabled;
    eb::PresentationPipeline pipeline(Time{}, settings, eb::FramePacer::frame_rate, cap, true);
    std::vector<std::uint32_t> pixels(8 * 224, 0xff203040);
    const auto publish = [&](unsigned frame) {
        auto scene = std::make_shared<eb::DirectSceneFrame>();
        scene->frame = frame; scene->width = 8; scene->scene_identity = 1;
        scene->motions = {{1, float(frame), 0}};
        eb::PresentationFrame view{};
        view.pixels = pixels; view.width = 8; view.frame = frame; view.scene = std::move(scene);
        pipeline.completed_frame(view);
        return view;
    };
    publish(0);
    Result result;
    Time now{}, last_sample{};
    double last_position{};
    bool sampled = false;
    while (now < Time{} + 6s) {
        if (pipeline.simulation_due(now)) {
            result.max_tick_lateness_ms = std::max(result.max_tick_lateness_ms,
                std::chrono::duration<double, std::milli>(now - (Time{} + period * result.ticks)).count());
            now += update_costs[result.ticks % update_costs.size()];
            ++result.ticks;
            pipeline.simulation_finished(publish(result.ticks), 1, now);
        }
        if (pipeline.presentation_due(now)) {
            const auto picture = pipeline.picture(now);
            require(picture.scene && picture.scene->offsets.size() == 1, "Scheduler lost a continuous scene");
            const double position = picture.scene->artwork->motions[0].x + picture.scene->offsets[0].x;
            if (sampled && result.ticks > 30) {
                const double expected = double((now - last_sample).count()) / period.count();
                const double error = std::abs((position - last_position) - expected);
                result.max_motion_error = std::max(result.max_motion_error, error);
                result.motion_error_sum += error / expected;
                result.holds += position == last_position;
                ++result.samples;
                result.max_draw_gap_ms = std::max(result.max_draw_gap_ms,
                    std::chrono::duration<double, std::milli>(now - last_sample).count());
            }
            last_position = position; last_sample = now; sampled = true;
            now += draw_cost;
            pipeline.presented(now, draw_cost);
            ++result.draws;
        }
        const auto wake = pipeline.wake_time(now);
        require(wake >= now, "Scheduler requested a wake in the past");
        now = wake;
    }
    std::cout << "cap=" << cap << " guard=" << guard_enabled << " update_us=" << update_costs[0].count() << ',' << update_costs[1].count()
              << " draw_us=" << draw_cost.count() << " ticks=" << result.ticks << " draws=" << result.draws
              << " max_tick_lateness_ms=" << result.max_tick_lateness_ms
              << " max_draw_gap_ms=" << result.max_draw_gap_ms
              << " mean_velocity_error_pct=" << 100 * result.motion_error_sum / result.samples
              << " holds=" << result.holds << '\n';
    return result;
}

void budgets_and_wake() {
    const auto period = eb::FramePacer::period();
    // A sudden expensive draw affects the very next decision. Both methods
    // must use the same budget; waking inside an excluded slot would spin.
    eb::PresentationClock peak(Time{}, 300);
    peak.simulated(1);
    peak.presented(Time{} + 50us, 50us);
    peak.presented(Time{} + 4ms, 2ms);
    const auto blocked = Time{} + period - 1ms;
    require(!peak.presentation_due(blocked) && peak.wake(blocked) == Time{} + period,
            "Cost spike did not immediately defer a conflicting draw");

    eb::PresentationClock recovered(Time{}, 0);
    recovered.simulated(1);
    auto now = Time{} + 2ms;
    recovered.presented(now, 2ms);
    for (unsigned i = 0; i < 80; ++i) {
        now += 50us;
        recovered.presented(now, 50us);
    }
    const auto cheap_slot = Time{} + period - 100us;
    require(recovered.presentation_due(cheap_slot) && recovered.wake(cheap_slot) == cheap_slot,
            "Old expensive draw kept taxing cheap presentation");

    eb::PresentationClock stalled(Time{}, 300);
    stalled.simulated(4);
    stalled.presented(Time{} + 40ms, 40ms);
    require(stalled.presentation_due(Time{} + 44ms), "Long swap produced an unbounded presentation exclusion");
    const auto resumed = Time{} + 1s;
    stalled.resume(resumed);
    stalled.simulated(1);
    require(stalled.presentation_due(resumed + period - 1us), "Resume retained an obsolete cost estimate");
    require(stalled.wake(resumed + 2 * period) == resumed + 2 * period,
            "Overdue simulation returned a stale wake deadline");
}
} // namespace

int main() {
    try {
        budgets_and_wake();
        const auto unguarded = run(300, {8000us, 8000us}, 500us, false);
        require(unguarded.ticks == 361 && unguarded.draws == 1239 && unguarded.holds == 0,
                "Disabled experiment changed the original presentation schedule");
        for (const auto cap : {90., 120., 144., 165., 240., 300., 0., 59.4, 118.8}) {
            for (const auto costs : {std::array{100us, 100us}, std::array{8000us, 2000us},
                                     std::array{8000us, 8000us}}) {
                for (const auto draw : {50us, 500us, 2000us}) {
                    const auto r = run(cap, costs, draw);
                    require(r.ticks == 361, "Presentation budget changed simulation tick accounting");
                    require(r.max_motion_error < 0.0001 && r.holds == 0,
                            "Scheduling variable-cost updates introduced a motion jump or hold");
                    require(r.max_tick_lateness_ms < 0.001, "Known presentation work crossed a native tick deadline");
                    if (cap == 300 && costs[0] == 8ms && costs[1] == 8ms && draw == 500us)
                        // The unguarded v0.4 fixture submits 1,239 pictures.
                        require(r.draws >= 1239 * 95 / 100,
                                "Adaptive draw budget lost more than 5 percent of cheap presentation slots");
                }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
