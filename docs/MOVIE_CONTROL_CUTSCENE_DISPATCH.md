# Cutscene dispatch boundary

The project does not yet have a verified normal-path cutscene dispatcher. The static intro fallback does not dispatch commands, update a timeline, start audio, or report completion.

A future dispatcher must use project-owned state, deterministic update ordering, explicit failure handling, and an admitted scene session. This page omits original component identities, callback order, event values, and timing observations.
