# Intro sound runtime

This page no longer publishes source-derived sound records, cue mappings, or exact playback values.

The audio runtime owns decoded streams and channel state for the lifetime of the scene. It must handle incremental decoding, bounded buffering, readiness, updates, stop, and teardown without tying simulation state to presentation audio. Optional soundtrack data must not be required for launch. Synthetic audio fixtures should verify lifecycle and failure handling without containing retail media.

An initialized sound owner can be prepared again after it is stopped. Preparing
an already active owner is rejected before changing the backend, avoiding
duplicate records. Synthetic tests cover stop, failed preparation and retry;
these checks do not establish intro audio playback.

The deferred sound reader accepts only work owned by its runtime queue before
it changes a backend record or admits coverage. Synthetic tests also verify
that rejecting a copied work item leaves the normal reader path usable.
