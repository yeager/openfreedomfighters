# Timing and portable simulation policy

OpenFreedomFighters keeps presentation timing separate from authoritative
simulation timing. Platform clocks, frame pacing, graphics settings, and audio
must not silently redefine simulation updates.

The portable simulation uses an explicit fixed-step policy. Input is assigned to
ticks, long presentation stalls are bounded, and interpolation is presentation
only. Any future compatibility timing mode must remain a separate interface with
its own source-free contract; it must not be inferred from display refresh or a
busy-wait interval.

## Verification gates

Tests for project timing policies should cover initialization, elapsed-time
boundaries, stalls, pause/resume, and input ordering using synthetic clock
samples. Compatibility claims require an approved source-free behavior contract
and independent validation. Do not publish executable-analysis details, raw
observations, or retail-derived timing vectors in this document.
