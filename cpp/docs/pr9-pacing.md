# Frame-pacing follow-up to PR #9

This integration builds on [Silent's PR #9](https://github.com/TheRunaway5/Phase-Distorter/pull/9)
at `3cee5742c9046159143dc9800ea9626b645f5833`, integrated onto v0.4
(`cf1e867ae6c187f82c6b759078ca76fc32ff45e9`). The three original commits and
their author attribution are retained. The useful change is reserving time for presentation
before a simulation deadline. Interpolation does not need a new host-time phase
estimate to use that guard.

## Changes on top

The draw guard is available as **F1 > Debug > Experimental frame pacing** and
defaults **off**. The preference is saved across restarts and game switches.
It also remains available in native sessions, whose machine-debug cheats are
disabled. Switching it on or off never rebases the simulation clock or discards
interpolation history. At the Native frame-rate setting it has no effect.

- Keep interpolation on the native timeline. The PR's moving phase estimate
  could publish a fast update's new endpoints before their blend window began,
  producing a jump followed by a hold. Retaining the existing timeline avoids
  that regression without adding a presentation queue or interpolation phase delay.
- Measure the entire presentation operation in the desktop composition root,
  including picture sampling, diagnostics, CPU/GL drawing and buffer swap.
  Timing only `SDL_GL_SwapWindow` misses expensive work before the swap.
- Feed the measurement through `PresentationPipeline::presented` alongside the
  completion time, rather than adding a separate display getter/report call.
- Raise the cost estimate immediately; reduce it by one eighth toward subsequent
  cheaper costs. Add 12.5% headroom, bounded to a quarter native period, instead
  of excluding twice an averaged swap cost. A long stall cannot leave a huge
  exclusion behind. Resetting modes, restoring a snapshot or resuming after a
  long suspension resets the estimate.
- Share draw eligibility between the due/wake decisions and never return a wake
  in the past. Deferred slots wait for simulation; the simulation clock itself
  is never postponed or accelerated by presentation.
- Correct the alternating-cost test to check an even iteration as well as an
  odd one. Add actual moving-scene coverage across endpoint promotion, plus a
  scheduler matrix, cost-spike/recovery checks and monotonic wake fixtures.

## Deterministic comparison

Each row runs six synthetic seconds through the actual pipeline and direct
scene-motion implementation, targeting 300 FPS with 0.5 ms presentation work.
All versions execute 361 simulation ticks. The PR comparison reports the full
0.5 ms as swap time, so both guards observe the same input cost.

| Version | Draws, steady 8 ms updates | Draws, alternating 8/2 ms | Motion holds, alternating | Maximum tick lateness |
| --- | ---: | ---: | ---: | ---: |
| v0.4 baseline | 1,239 | 1,518 | 0 | 0.492 ms |
| PR `3cee574` | 1,134 | 1,457 | 109 | 0 ms |
| Follow-up | 1,182 | 1,490 | 0 | 0 ms |

Mean relative velocity error in the alternating case is 23.7962% for the PR and
0.00394% for the follow-up (0.00368% baseline; small residuals come from float
scene positions). This fixture retains about 95.4% of baseline steady-work draw
submissions while eliminating its known draw-induced tick lateness. It does not
promise that every requested presentation slot is affordable on real hardware.

A separate fixture models 2 ms total presentation work with only 50 us spent in
the swap. The PR's swap-only budget allows up to 1.882 ms tick lateness and
submits 1,070 pictures; the full-work guard eliminates that lateness but submits
836. Measuring the full operation makes the tradeoff explicit; protecting tick
deadlines can cost substantial throughput when presentation itself is expensive.

The regression tests were run before the production fix: the corrected PR clock
test failed with `Alternating update costs changed the motion rate`, and the new
pipeline test failed with `Variable update cost introduced a scene-motion jump
or hold`. Both pass with the follow-up.

Run the focused checks from a configured build:

```sh
cmake --build build --target frame_pacer_tests presentation_pipeline_tests \
  presentation_scheduler_tests frame_interpolator_tests photosensitivity_filter_tests \
  direct_scene_tests --parallel 4
ctest --test-dir build --output-on-failure \
  -R '^(frame_pacer_tests|presentation_pipeline_tests|presentation_scheduler_tests|frame_interpolator_tests|photosensitivity_filter_tests|direct_scene_tests)$'
./build/cpp/presentation_scheduler_tests
```

These are deterministic scheduling and position checks, not a physical scanout
measurement. Native frame-rate mode and simulation/input/audio execution retain
their existing policies. The bounded guard can still be exceeded by unforeseen
rendering cost, driver blocking or host load. Source frames that arrive too late
still hold at an endpoint; this change does not invent motion beyond source data.

## Prototype frontend checks

The six focused CTest suites pass, including the 81-case scheduler matrix.
The changed desktop translation units compile successfully. For a bounded
runtime comparison, the rebuilt application and presentation library were linked
against the unchanged v0.4 engine libraries; this was not a clean whole-project
rebuild or release package.

On the NVIDIA RTX 5080 desktop, 360-frame US and JP startup runs at a 300 FPS
limit, immediate swaps and audio playback disabled produced identical final
CPU/SPC diagnostics, instruction counts, DSP sample counts, WAV bytes and native
framebuffer captures compared with v0.4. Tests used `--no-config --no-save` and
separate output paths; user preferences and saves were not written. The
integrated PR frontend also matched the US result.

Observed submissions varied with host load: the follow-up's US runs measured
250.1 and 298.8 FPS, while the baseline measured 268.9 and the PR 261.8. The PR
run accumulated 18 catch-up frames, versus zero in those follow-up runs. The JP
follow-up measured 250.0 FPS versus 265.8 baseline, both without catch-up frames.
These short, unpaired-load startup captures establish bounded frontend execution
and unchanged game output, not a reliable FPS improvement, sustained moving
gameplay, audio-device behavior, VRR scanout, or full-game validation. Keep the
guard experimental until representative gameplay traces establish the preferred
tradeoff between submitted pictures and simulation lateness.

MinGW builds of `frame_pacer_tests` and `presentation_scheduler_tests` also pass
under an isolated Wine prefix, including the same 81 scheduler combinations.
These verify portable duration arithmetic and scheduling decisions, not native
Windows timer, audio-device, driver or display behavior. No Windows application
package was built for this proposal.

## Integrated launcher checks

The integrated feature defaults off and is enabled through **F1 > Debug >
Experimental frame pacing**. Its saved preference and live on/off behavior are
covered by the desktop-support and pipeline tests. The SDL/ImGui test navigates
to the real Debug checkbox, clicks it on and off, verifies that closing the
panel retains it, and exercises availability with native-session machine cheats
disabled.

The normal CMake application builds and `refresh_launchers` targets succeed for
Linux and MinGW Windows. Both canonical launcher executables are refreshed.
Eight focused Linux CTest suites and five Windows/Wine suites pass, including
the UI test, 81 guarded scheduler cases and the unguarded v0.4 scheduling fixture.

Using the refreshed Linux launcher, 360-frame US and JP startup runs with the
guard off/on retain identical CPU/SPC state, instruction and DSP sample counts,
WAV bytes and native framebuffer captures; each run retains its chosen saved
preference. Separate 120-frame US and JP headless launcher checks also produce
identical native captures and WAV output on Linux and Windows/Wine. These
bounded checks do not establish physical Windows/VRR behavior, full-game parity
or a consistent FPS improvement under varying host load.
