# Scene transform boundary

The normal world-scene producer is not yet connected to an admitted runtime scene. The diagnostic renderer must remain labelled as diagnostic; it is not a gameplay transform or a parity reference.

Map records, runtime object identity, parent relationships, camera state, accepted draw records, and backend submission must be joined by explicit project-owned interfaces before a normal world frame can be produced. The current renderer keeps source and map transform inputs separate and does not claim that their composition matches the game.

## Work required

- Establish a clean-room behavior contract for how map entries identify live scene objects and their parents.
- Specify how the active view and camera reach accepted draw records.
- Produce world-space transforms and ordered draw records from the same admitted scene state.
- Keep diagnostic rendering separate from normal scene admission and temporal-upscaler inputs.

This document intentionally omits executable analysis, binary layouts, offsets, retail values, and corpus measurements. See [CLEAN_ROOM.md](../CLEAN_ROOM.md) for publication rules and [MODERN_GRAPHICS.md](MODERN_GRAPHICS.md) for the current renderer boundary.
