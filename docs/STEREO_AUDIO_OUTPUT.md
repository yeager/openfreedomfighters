# Intro stereo playback service (local work in progress)

The logical backend retains one master gain, separate from its category gains.
Sound records retain pan, producer-written priority and grouping state. The
frequency adjustment uses the second existing timing-change integer; grouping
count replaces the provisional repetition-count name. Neither has a duplicate
shadow field. Unassigned priority and environment indices stay optional.

The explicit command builder consumes an already admitted priority-ordered
binding list. It visits at most 65 entries, including skipped bindings. Valid
simple SND links produce nonspatial commands; state 5 becomes state 1 with a
start request. Muted records still produce commands. Missing SND/WHD references
produce diagnostics, not guessed bank rows or playback acknowledgements.

Command gain uses separate binary32 category/master and record gain/multiplier
products. Channel volume conversion is logarithmic and separate from the
category response curve. Pan clamps to [-10000,10000]. Frequency uses the
reviewed signed adjustment arithmetic followed by integer clamping to
[100,100000] Hz. Nonfinite arithmetic and unrepresentable signed conversions
are explicit native failures. See [source commands](STEREO_SOURCE_COMMANDS.md).

## Command, decoder and output connection

`StereoStreamPlayback` owns active channels in a fixed slot order. Each channel
retains its selected WHD record, opened bank reader, incremental Vorbis decoder
and output device. `make_sdl_intro_audio_playback` connects WHD links to the
owned intro bank and creates real SDL outputs; it has no silent fallback.

New nonloop commands without a start request do not allocate. Active bindings
receive controls without restarting or replacing their source. The service
polls real initial reads, observes completion, requests bounded PCM chunks and
submits them as the output consumes its input queue. It applies pan, volume
and frequency to the device. Integer hundredths-dB volume is adapted to SDL
linear amplitude; -10000 means silence. This amplitude conversion uses native
math, not a bit-exact implementation of the original audio API.

Initial device start follows real PCM submission and queues one start binding.
This boundary does not require the first consumed frame, but is not proof of
audibility. A held channel cannot start; clearing a pre-start hold defers
playback until PCM readiness. Resuming an already-started channel preserves its
queued position and produces no second acknowledgement.

An explicit stop closes output and cancels/joins the old decoder before its
slot can be reused. Pending starts survive stop. The receive payload retains
separate sequences: deliver all starts, then all stops. Stopped-binding receiver
effects are not implemented by this service. Output/decoder failures propagate
and retire the affected channel, without manufacturing a successful event.

Decoder EOF flushes the final PCM but does not destroy the channel, claim
hardware completion, or send a scene event. The current policy retains output
until ordered stop or teardown. A zero input-queue count alone is not a drain
or hardware-completion signal. Canonical prepared-record expiry must still run.

Defaults are native resource limits: 65 channels, 16,384 queued stereo frames
and 4,096 decoded frames per request. Each pending notification sequence is
bounded to 4,096 entries. Looping, source replacement without stop, nonnegative
environment groups and priority eviction at capacity fail explicitly; these
are outside the supported cold intro channel path.

## SDL device boundary

`SdlAudioOutput` owns an initially paused signed-16 stereo stream and logical
device. It submits bounded PCM, reports queued input bytes, applies gain and
frequency, resumes, pauses, flushes and stops. Destruction closes the stream and
balances its audio-subsystem reference. All instances must be destroyed before
global SDL shutdown. It is a manager-thread API, not an audio callback.
These ownership and pause semantics follow the
[SDL device-stream contract](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream).

The actual cold intro pan is zero. Nonzero pan and optional spatial effects are
explicitly unsupported by this output adapter. The stream uses a fixed 10 kHz
logical input rate: the admitted legacy 100--100,000 Hz range maps exactly to
SDL ratios 0.01--10. SDL consumes those ratios continuously, so controls can
change after PCM has been queued without a format change or silent clamp. See
[SDL frequency ratio](https://wiki.libsdl.org/SDL3/SDL_SetAudioStreamFrequencyRatio).
Production refuses dummy/disk drivers. No adapter method emits a playback ACK.

## Verification status

The real-device test uses generated PCM and requires the output queue to drain.
Unavailable devices return a test skip; an opened device that stalls fails.
Dummy-driver rejection is a separate synthetic test, not playback evidence.
An unbound SDL conversion-stream test also generates a 44.1 kHz 1 kHz tone and
verifies that the 10 kHz logical-rate composition with a 4.41 ratio preserves
both its approximate one-second duration and its 1 kHz zero-crossing count.
This proves the adapter's rate composition without making an audibility claim.

On the current development host, SDL 3.4.2 opens and resumes real playback
drivers but input consumption stalls. Minimal direct SDL probes reproduce the
failure without this adapter: PipeWire consumes none of the 17,640 input bytes;
ALSA and PulseAudio consume only a prefix. Both silent and very-low-amplitude
nonzero PCM reproduce the PipeWire failure. PulseAudio teardown can also hang;
the test has a bounded timeout. This isolates the observed failure below the
adapter, but does not by itself distinguish an SDL defect from backend/host
behavior. Read-only ALSA diagnostics add evidence of a stalled underlying
playback clock: hardware and application pointers remained at 0 and 2823
across observations despite RUNNING state. No audio configuration was changed.
Continuous playback is not verified here.

A private probe of both hash-verified retail intro streams reached the same
failure through the complete new command/decoder/output connection. Each
channel submitted 16,384 actual PCM frames, retained a 65,536-byte input queue,
and read 65,536 encoded bytes. Two successful SDL starts produced two distinct
start notifications; ordered stops produced two stop notifications. No scene
receive was invoked and canonical progress remained zero. The three-second
probe failed its output-progress check, rather than treating admission as
continuous playback. Retail PCM was not written or added to the repository.

Normal startup does not yet call this service. Listener/grouping admission,
complete batches, scene receive delivery and prepared-record expiry/stop must
still be connected before the real intro can drive it. Tests with controlled
admission are not evidence that those scene gates have run.
