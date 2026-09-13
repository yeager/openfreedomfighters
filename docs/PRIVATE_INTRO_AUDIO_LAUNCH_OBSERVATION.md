# Private intro-audio launch observation

This is a clean-room, private-only procedure for learning which already
inventoried intro audio streams are accessed before the original reaches its
first interactive menu. It does not identify a cue, prove playback, infer a
clock, or authorize native audio playback.

After any existing original process has exited naturally, run an isolated,
owned Steam/Proton launch under a launch-time system-call tracer. Trace child
processes and only file-open results; do not attach to an existing process or
modify game files. Keep the raw tracer output and every pathname private.

A separate private parser may compare each raw path only against the locally
verified intro-stream inventory. It must emit a new JSON input for
`tools/intro_audio_launch_access_trace.py` with this exact shape:

```json
{
  "format": "off.intro-audio-launch-access.raw/v1",
  "fresh_isolated": true,
  "events": [
    {"observation_order": 0, "intro_stream_ordinal": 1,
     "bank": "local", "access_result": "opened"}
  ]
}
```

Ordinals are opaque, observer-local positions in that private inventory.
`bank` is only `local` or `global`; `access_result` is only `opened`,
`missing`, or `denied`. The parser must discard every unmatched access and must
not emit paths, names, hashes, bytes, text, timestamps, process details,
addresses, offsets, or executable data.

Sanitize the private JSON to a new private output file. The sanitizer rejects
non-fresh runs, unexpected fields, duplicate access results, and unsafe input
or output paths. Delete the raw tracer log under the operator's retention
policy after independent review. Do not commit either raw or sanitized
observation to the repository.

The reviewed result can only establish launch-time stream access. A separate
repeatable cue/playback observation remains necessary before associating a
stream with a runtime cue or enabling intro music.
