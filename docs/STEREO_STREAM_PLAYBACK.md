# Intro stereo channel service

`StereoStreamPlayback` connects admitted source commands to retained bank
readers, incremental decoding, and a host-provided `StereoPcmOutput`. It does
not own scene admission, listener selection, or the scene update loop.

The service retains each reader and decoder for its channel lifetime. It submits
bounded PCM chunks as output capacity allows and keeps command resolution,
channel admission, and receive delivery as separate interfaces. Device start is
an admission event, not proof of audible output. Stop and teardown cancel and
join decoder work before a channel is reused. End of input does not imply that
queued audio has completed; lifecycle and stop delivery remain explicit.

Project-owned native limits bound channels, queued frames, decoded requests, and
pending notifications. Looping, grouped effects, priority eviction, and source
replacement without a stop are unsupported until explicitly implemented.
These limits describe this service only.

## Verification

Tests should use generated audio fixtures and synthetic output. They can cover
incremental decoding, backpressure, lifecycle transitions, end-of-input, and
failure cleanup without storing user installation audio. Device-specific smoke
checks do not establish audible output on other systems. Scene admission,
complete command batches, and expiry/stop integration remain separate work.
