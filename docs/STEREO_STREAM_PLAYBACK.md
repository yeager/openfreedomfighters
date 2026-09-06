# Intro stereo channel service

`StereoStreamPlayback` connects admitted source commands to retained bank
readers, incremental Vorbis decoding and a concrete `StereoPcmOutput` supplied
by the host. It does not admit listeners/groups or run the scene update loop.

New nonloop commands without a start request do not allocate. Existing bindings
receive pan, volume and frequency controls without restarting. New channels
occupy the first free slot. The manager polls actual encoded-read completion,
observes initial readiness, then submits bounded PCM chunks as the output's
input queue permits. Commands resolve their WHD link, not a guessed sound-list
index. The opened reader and decoder remain attached to that channel lifetime.

Initial output start requires submitted PCM and produces one pending start
binding. This is device admission, not proof of audibility. Holding before
initial start prevents playback; resuming a started channel preserves queued
position and produces no second start notification. Stop closes output and
cancels/joins the decoder before slot reuse. Pending starts survive stop:
receive delivery processes all starts first, then the separate stopped sequence.
The service does not implement stopped-message receiver effects.

EOF flushes final PCM but does not destroy queued audio or send a scene event.
Channels remain until an ordered stop or teardown. Zero queued input is not
hardware completion. Canonical prepared-record expiry and its stop commands
still have to run. Output/decoder errors propagate and retire the affected
channel without a fabricated success notification.
Nonfinite/unrepresentable controls are rejected native inputs. Their partial
failure side effects are not claimed to reproduce legacy API error handling.

Native limits default to 65 channels, 16,384 queued stereo frames, 4,096 decoded
frames per request, and 4,096 pending notifications per kind. Looping, grouped
effects, changing an active source without stop, and priority eviction are
explicitly unsupported here. This covers the two cold nonloop intro streams,
not the entire game's audio system.

## Verification

Tests use generated Vorbis data and an explicitly synthetic recording output.
They check complete PCM against offline decoding, backpressure, initial/held
starts, updates, stop/restart, EOF retention and failure cleanup. A private
comparison using both retail intro tracks also matched every sample through
the manager: 1,042,524 and 6,029,352 stereo frames. No retail PCM was saved or
published. Neither recording sink is evidence of audible output.

The real SDL adapter remains unverified local work: this host opens playback
devices but their queues stall, including a probe using both actual intro
streams. Normal startup does not call this channel service. Listener/grouping
admission, complete batches, actual scene receive delivery and expiry/stop
integration remain required before this can drive the intro.
