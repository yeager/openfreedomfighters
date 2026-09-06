# Application clock and sound preferences

`ApplicationServices` owns the clock, application text configuration, sound
preferences and canonical sound-record registry. `IntroRuntime` borrows it: constructing a scene does not reset the
application. The owner must outlive the scene and all callbacks bound from it.

Normal startup resets this clock before CPU resource loading. The retained
intro controller's phase-two entry point binds clock and volume operations to
this same owner, preserving caller-supplied input, global numeric-property and
renderer services. Normal startup does not invoke phase two or run the scene
update loop yet. The logical sound backend has no output device or channel pump.

## Clock

The initial rate is 1 and CRT mode is off. Selecting CRT mode only assigns the
mode; it does not sample time. Reset clears accumulators, integer clocks, scene
offset, raw delta and smoothing. It retains rate, mode, suppression, last scaled
increment and last scene delta. Scene freeze flags and the freeze snapshot belong
to the caller; reset does not clear them. The recovered cold loader's separate
snapshot clear is not part of this generic operation.

Reset and rebase sample the alternate counter first, then CRT milliseconds.
Rebase preserves accumulated and published time. Each CRT advance updates raw
time before sampling the alternate counter, even when scaled accumulation is
suppressed. Float-rounding boundaries and truncation to 1024 integer units per
second are explicit. Suppression retains the previous scaled increment and
accumulated scaled time, then clears itself.

Scene publication is separate from the upstream producer. Freeze retains scene
time and publishes zero delta; resume adjusts the offset for the frozen interval.
Scene offset arithmetic preserves 32-bit word behavior. This does not make the
controller's signed deadline comparison wrap-safe.

The native sampler uses `steady_clock`, not POSIX process CPU time. It supplies
nonnegative, nondecreasing signed 32-bit millisecond samples. Out-of-domain
samples, non-finite values and conversion overflow fail explicitly. This is a
portable policy, not a Windows reference capture. Alternate-mode advancement,
counter-failure fallback and timing recording/replay are not implemented.
The caller explicitly selects the no-recording-or-replay policy.

Sampling failures retain the completed state prefix and poison the clock;
retrying a partially completed advance is not supported. Missing services are
rejected before mutation.

## Sound preferences

Volume starts at 90. Initialization from an already-parsed setting clamps to
0–100; text parsing, duplicate-key handling and configuration loading are not
implemented by this boundary.

A changed volume request stores the signed interpretation of the supplied
32-bit word without clamping. It updates `SoundEffectsVolume` as signed decimal
text in the preference and application stores, clearing their deletion markers.
It then resolves the live backend and, if present, requests category 0 with the
then-current volume and mode 2. An equal request has no effects. Failures retain
completed effects; the setter does not promise rollback or recovery.

These text stores are not the global numeric properties used by controller
initialization. Backend absence does not imply absent global properties and does
not emit a sound-ready event. By default, requests now reach the same backend
that retains intro sound records. Its mode-2 response is independently
reconstructed, including category selection and pending-update state; see
[intro sound runtime](INTRO_SOUND_RUNTIME.md). An explicitly supplied resolver
remains available for external service injection. There is no disk writer or
music playback.

## Verification

Independent fixtures cover reset/rebase ordering, rounded deltas, suppression,
freeze/resume, failure prefixes and overflow rejection. Sound tests cover signed
words, equal requests, configuration ordering and live backend changes. The
retained-intro integration test checks shared clock and volume state across two
scene owners, including controller deadline assignment.

The integration fixture supplies synthetic global/input/renderer services. It
does not establish original component admission, real presentation or automatic
intro playback.
