# Scene transform boundary

The normal world-scene producer is not yet connected to an admitted runtime scene. The diagnostic renderer must remain labelled as diagnostic; it is not a gameplay transform or a parity reference.

Map records, runtime object identity, parent relationships, camera state, accepted draw records, and backend submission must be joined by explicit project-owned interfaces before a normal world frame can be produced. The current renderer keeps source and map transform inputs separate and does not claim that their composition matches the game.

## Authored scene ownership

Scene object hierarchy follows authored containment. Objects nested beneath a group are attached to that group; nested groups own their descendants. Returning to an enclosing level resumes that enclosing group as the parent.

This establishes parent/child ownership only. It does not establish which runtime object an RMC/RMI map reference selects, how map-derived transforms combine with the parent-aware pose, or which camera/view state applies to a given draw.

On one reviewed Direct3D draw path, submitted item positions derive from an object pose that incorporates its authored parent hierarchy. The role of this draw path is unresolved; it is not established as the normal world-scene geometry pass. This does not establish which runtime object an RMC/RMI map reference selects, how map-derived transforms combine with the parent-aware pose, how shaders interpret submitted data, or whether other backends behave equivalently.

Geometry candidates are collected from an already constructed runtime scene hierarchy. The relationship between authored map references and those candidates remains unverified; this does not establish that the collection is exhaustive or represents the normal gameplay scene.

`bind_scene_instances_to_runtime` now provides a narrow identity/resource receipt after complete runtime directory construction. It checks the source-index resource mapping, canonical owner association, source-pose agreement, and any authored parent recorded by the asset against the live runtime parent. The caller must supply an asset built from the same GMS directory as the runtime and call before gameplay updates change source poses. This receipt does not establish map placement, world transforms, camera/view selection, visibility, draw order, or backend acceptance.

## Work required

- Join map entries to live scene-object identities and their established parent ancestry.
- Specify how the active view and camera reach accepted draw records.
- Produce world-space transforms and ordered draw records from the same admitted scene state.
- Keep diagnostic rendering separate from normal scene admission and temporal-upscaler inputs.

This document intentionally omits executable analysis, binary layouts, offsets, retail values, and corpus measurements. See [CLEAN_ROOM.md](../CLEAN_ROOM.md) for publication rules and [MODERN_GRAPHICS.md](MODERN_GRAPHICS.md) for the current renderer boundary.
