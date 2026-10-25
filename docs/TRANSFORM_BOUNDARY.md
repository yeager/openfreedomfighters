# Scene transform boundary

The normal world-scene producer is not yet connected to an admitted runtime scene. The diagnostic renderer must remain labelled as diagnostic; it is not a gameplay transform or a parity reference.

Map records, runtime object identity, parent relationships, camera state, accepted draw records, and backend submission must be joined by explicit project-owned interfaces before a normal world frame can be produced. The current renderer keeps source and map transform inputs separate and does not claim that their composition matches the game.

## Authored scene ownership

Scene object hierarchy follows authored containment. Objects nested beneath a group are attached to that group; nested groups own their descendants. Returning to an enclosing level resumes that enclosing group as the parent.

This establishes parent/child ownership only. It does not establish which runtime object a map reference selects, how candidate transforms for geometry submission compose, which camera is active, or how the active view reaches accepted draw records and backend submission.

View-based visibility preparation evaluates candidate-object transforms in the context of the view and the candidate's enclosing hierarchy. This establishes a visibility relationship only; it does not establish that the resulting transform reaches geometry submission or the shader draw matrix.

## Work required

- Join map entries to live scene-object identities and their established parent ancestry.
- Specify how the active view and camera reach accepted draw records.
- Produce world-space transforms and ordered draw records from the same admitted scene state.
- Keep diagnostic rendering separate from normal scene admission and temporal-upscaler inputs.

This document intentionally omits executable analysis, binary layouts, offsets, retail values, and corpus measurements. See [CLEAN_ROOM.md](../CLEAN_ROOM.md) for publication rules and [MODERN_GRAPHICS.md](MODERN_GRAPHICS.md) for the current renderer boundary.
