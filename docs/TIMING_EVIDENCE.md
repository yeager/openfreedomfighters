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
smoothed_delta = 0.7 * previous_smoothed_delta
               + 0.3 * bounded_sample
exposed_delta  = time_scale * min(0.1, smoothed_delta)
```

The retained smoothed value is stored before the final 100 ms clamp. A separate
selectable path exposes a scaled millisecond-resolution process-clock delta
without this high-resolution smoothing. The available evidence does not yet
establish which modes select that path, so it is not described as a fixed step.

The timing service also has a one-shot suppression operation. On the next
update, it leaves the scaled delta and scaled total unchanged, then clears the
suppression state. A caller relationship to every pause or menu transition has
not been proven.

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
| 0.0 | 2.0 | 1.0 | 0.3 | 0.1 |
| 0.02 | 0.04 | 1.0 | 0.026 | 0.026 |
| 0.02 | 0.04 | 0.5 | 0.026 | 0.013 |

Tests must also prove that one-shot suppression preserves the previously
exposed delta and scaled total for exactly one update, while clock samples can
still be refreshed for the following update. These vectors specify only the
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
