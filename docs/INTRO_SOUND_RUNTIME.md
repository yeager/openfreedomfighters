# Intro sound runtime

This page no longer publishes source-derived sound records, cue mappings, or exact playback values.

The audio runtime owns decoded streams and channel state for the lifetime of the scene. It must handle incremental decoding, bounded buffering, readiness, updates, stop, and teardown without tying simulation state to presentation audio. Optional soundtrack data must not be required for launch. Synthetic audio fixtures should verify lifecycle and failure handling without containing retail media.
