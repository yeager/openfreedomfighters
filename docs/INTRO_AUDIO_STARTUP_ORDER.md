# Intro audio boundary

The project has portable audio decoding and output services, but normal intro startup does not yet map scene events to verified playback or completion. Decoding a source does not establish cue timing, listener state, or audible scene playback.

Audio start, update, stop, and teardown must be driven by the owning scene lifecycle. Audio must continue to fail safely when optional sources or output devices are unavailable, and scene teardown must stop active playback before releasing its source resources.

This document intentionally omits retail audio data, cue mappings, offsets, and private analysis. See [INTRO_AND_MENU_PLAN.md](INTRO_AND_MENU_PLAN.md), [INTRO_SOUND_RUNTIME.md](INTRO_SOUND_RUNTIME.md), and [CLEAN_ROOM.md](../CLEAN_ROOM.md).
