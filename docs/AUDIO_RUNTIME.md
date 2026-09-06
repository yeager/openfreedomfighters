# Audio mixing and positional runtime specification

This document defines the Phase 2 runtime downstream of the bounded audio-bank
reader and portable decoder in [AUDIO_FORMAT.md](AUDIO_FORMAT.md). PCM, IMA ADPCM,
and single-link Ogg Vorbis currently decode to owned interleaved signed 16-bit
samples with validated channels and sample rate. No audio device, voice scheduler,
mixer, resampler, positional renderer, environmental effect, or playback path is
implemented yet.

The retained sound-preference state and intro-controller volume binding are
implemented separately; see [application services](APPLICATION_SERVICES.md).
They do not open an audio device or establish the planned mixer behavior below.
The intro sound components require retained sound records during initialization;
backend absence is not a supported reason to replace their callbacks with no-ops.
Intro source-to-SND-to-WHD bindings now load from the owned installation, with
bounded on-demand decoding; see [audio formats](AUDIO_FORMAT.md#intro-source-binding).
Mutable runtime sound records and their lifecycle callbacks remain unimplemented.
Successful decoding is not a playback-start acknowledgement or a readiness event.

## Runtime boundary

Decoded samples are immutable assets. Playback creates bounded voice instances
with an asset reference, source-frame cursor, loop contract, gain,
pitch/resampling state, bus, priority, and optional emitter identity. Simulation
emits tick-addressed sound events; the audio thread consumes an ordered
presentation queue. Callback timing, underruns, device changes, and voice
stealing cannot feed back into authoritative simulation.

The mixer uses 32-bit float internally, accumulates with headroom, applies a
documented limiter, and converts once to the SDL device format. A deterministic,
bounded resampler converts source rates to device rate. Layouts beyond confirmed
mono/stereo inputs remain unsupported until specified. Streaming Vorbis uses
bounded ring buffers and background decode; the real-time callback performs no
file I/O, decode, allocation, contended locking, or logging.

## Buses and voices

The initial graph has master, music, dialogue, effects, ambience, and UI buses.
Each uses linear internal gain with a user-facing decibel control; mute is
distinct from minimum gain. Original-mode relative levels, concurrency groups,
loop points, fades, and priorities require clean-room evidence and are not
inferred from bank order or record identity.

Voice and stream counts are bounded. At capacity, a deterministic policy
considers protected dialogue/music, explicit priority, audibility, age, and
stable creation sequence. The exact Original policy remains an evidence task.
Duplicate-event suppression, if observed, is specified per family.

Pause freezes gameplay voices and fades on an output-frame boundary; UI feedback
remains available. Device loss suspends output without advancing cursors. Device
reopen rebuilds only presentation state and uses short ramps to avoid clicks.
Settings changes are ramped and do not restart music unless device reconstruction
requires it.

## Positional audio

The simulation/render snapshot supplies listener position and orthonormal
orientation plus emitter position, velocity, radius, and environment/occlusion
inputs. Coordinate conversion is centralized and tested against the recovered
world transform; provisional diagnostic transforms cannot establish Original
spatial behavior.

Mono effects may be spatialized. Stereo music, dialogue, and UI remain
non-positional unless evidence identifies another family. The portable baseline
provides recovered distance attenuation, equal-power stereo panning, bounded
Doppler pitch, smoothed per-emitter gain with teleport reset, and low-pass/gain
occlusion supplied by a later world query.

EAX imports establish an original environmental-audio boundary; they do not
justify requiring or redistributing an EAX runtime. Original maps validated
room/reverb behavior to a portable effect. Modern may offer higher-quality
spatialization but preserves event timing, source selection, and bus semantics,
with the portable stereo mixer as fallback.

## Fidelity, diagnostics, and acceptance

Private reference captures measure event timing, duration, channel behavior,
relative loudness, loop/fade boundaries, attenuation, panning, and room
transitions without publishing sampled audio. Comparisons use aligned envelopes
and declared tolerances.

Public diagnostics expose device format, buffer size, aggregate voice counts,
underruns, and decode latency. They never log retail paths/identifiers, bytes,
samples, dialogue text, or full source digests. User captures containing retail
audio remain local outside the repository; tests use authored tones and impulses.

Public tests prove interleaving, resampling length/phase bounds, gain/pan law,
limiting, loops/fades, voice stealing, pause/device recovery, stream starvation,
and positional invariants. The Phase 2 gate requires audible startup/menu/static
level playback on Windows, Linux, Steam Deck hardware, and macOS; deterministic
offline-mix hashes; no callback allocations or blocking operations; and private
timing/loudness/spatial comparisons within declared tolerances. Decoder tests
alone do not pass this gate.
