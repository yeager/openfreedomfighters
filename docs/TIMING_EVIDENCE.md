# Timing evidence and portable simulation policy

OpenFreedomFighters separates recovered timing behavior from its portable
deterministic simulation policy. This distinction prevents a presentation
clock observation from silently becoming a gameplay-fidelity claim.

## Recovered variable-delta behavior

A recovered application timing service has a high-resolution path that derives
an elapsed sample from consecutive performance-counter values and the counter
frequency. For a non-negative raw elapsed value `r`, its update is:

```text
bounded_sample = min(1.0, r)
seeded_previous = bounded_sample if previous_smoothed_delta == 0
                  else previous_smoothed_delta
smoothed_delta = 0.7 * seeded_previous
               + 0.3 * bounded_sample
exposed_delta  = time_scale * min(0.1, smoothed_delta)
```

This is a conceptual formula, not a bit-exact floating-point implementation.
The zero-state seeding step corrects the earlier formula, which incorrectly
blended the first sample against zero. The seed check applies whenever the stored
smoothing state is zero, not only at initialization. Counter differences and counter rate,
stored constants and increments have observed float-rounding boundaries; the
upstream accumulator is double precision. Exact operation rounding still requires
an implementation-ready contract.

The retained smoothed value is stored before the final 100 ms clamp. A separate
selectable path exposes a scaled millisecond-resolution process-clock delta
without this high-resolution smoothing. Intro controller phase two selects this
CRT delta path with an assignment-only mode change. It does not sample or reset
time. This does not establish the selected path for every gameplay mode and does
not make either path a fixed step.

The timing service also has a one-shot suppression operation. On the next
update it skips increment selection and accumulation, retaining the previous
scaled delta before later hooks, then clears the suppression state while other
per-frame sampling still occurs. The complete effects on every exposed value,
hook and caller have not been established; neither has its relationship to every
pause or menu transition.

## Scene clock used by intro controllers

The upstream rate-scaled increment is rounded to float and accumulated in double
precision. Its time value multiplied by 1024 is converted to a signed integer by
truncation. The scene clock adds a retained offset to that upstream integer.
Its published delta is the signed scene-clock difference scaled by 1/1024.

Scene freeze preserves the scene clock and publishes zero scene delta while the
upstream producer still runs. Resume compensates the offset for the upstream
interval spent frozen. The intro controller therefore observes engine time,
not a direct host millisecond counter. Its delay of 2048 units is nominally two
engine-time seconds; rate, smoothing, clamps, suppression and freeze prevent a
guaranteed wall-clock interpretation.

The signed deadline comparison is not a wrap-safe elapsed-time test. Observed
32-bit additions do not establish portable behavior for the upstream
out-of-range floating-to-integer conversion. Controller phase two assigns the
deadline after selecting CRT mode and running the audio-volume helper. Startup
rate is now established as 1. The CRT path, reset/rebase and scene publication
are implemented with an explicit checked native sampling policy; see
[application services](APPLICATION_SERVICES.md). Ordinary update admission and
the full alternate-clock and recording/replay paths remain incomplete.

## Frame pacing is not a simulation rate

A separate recovered routine polls a high-resolution clock until 0.016 seconds
have elapsed. The observed wait is a busy wait rather than a sleep. Flat control
flow does not establish that this routine runs for every gameplay frame, that it
is enabled in every mode, or that one wait corresponds to one simulation
update.

The 0.016-second threshold therefore does **not** prove a 62.5 Hz gameplay tick
rate. It must not be used as the portable simulation frequency without further
call-site and black-box evidence.

## Portable fixed-step policy

The native reimplementation uses a rational fixed-step scheduler so Original
and Modern consume identical tick-addressed input and produce the same
authoritative state. Presentation may interpolate between completed simulation
ticks, but display refresh, frame pacing, graphics menus, and renderer stalls do
not redefine simulation time.

This is a portability and determinism policy. Its current 60 Hz configuration
is not presented as recovered retail behavior. If later evidence requires a
different Original compatibility profile, the tick rate remains an explicit
simulation setting rather than being inferred from presentation pacing.

## Behavioral test vectors

Any future compatibility implementation of the recovered variable-delta path
must cover at least these independent cases:

| Previous smoothed | Raw elapsed | Scale | Next smoothed | Exposed delta |
|---:|---:|---:|---:|---:|
| 0.0 | 2.0 | 1.0 | approximately 1.0 | approximately 0.1 |
| 0.02 | 0.04 | 1.0 | 0.026 | 0.026 |
| 0.02 | 0.04 | 0.5 | 0.026 | 0.013 |

The decimal vectors are conceptual expectations, not bit-exact reference values.
Tests must also prove that one-shot suppression skips accumulation for exactly
one update, while clock samples can still be refreshed for the following update.
Assertions about other exposed values require further evidence. These vectors specify only the
recovered timing service; they do not replace the fixed-step scheduler tests.

## Remaining evidence gates

Before claiming Original timing fidelity, research must establish:

- the timing service's exact call position in the gameplay loop;
- simulation-update count and ordering relative to input and presentation;
- the conditions that select alternate clock and pacing paths;
- pause, focus-loss, loading, and menu ownership of delta suppression; and
- black-box behavior across normal frames and deliberate long stalls.

Raw executable addresses and instruction listings remain private clean-room
artifacts under the repository data policy.
