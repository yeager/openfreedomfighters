# Scene transform boundary

The normal world-scene producer is not yet connected to an admitted runtime scene. The diagnostic renderer must remain labelled as diagnostic; it is not a gameplay transform or a parity reference.

Map records, runtime object identity, parent relationships, camera state, accepted draw records, and backend submission must be joined by explicit project-owned interfaces before a normal world frame can be produced. The current renderer keeps source and map transform inputs separate and does not claim that their composition matches the game.

## Authored scene ownership

Scene object hierarchy follows authored containment. Objects nested beneath a group are attached to that group; nested groups own their descendants. Returning to an enclosing level resumes that enclosing group as the parent.

This establishes parent/child ownership only. It does not establish which runtime object an RMC/RMI map reference selects, how map-derived transforms combine with the parent-aware pose, or which camera/view state applies to a given draw.

On one reviewed Direct3D draw path, submitted item positions derive from an object pose that incorporates its authored parent hierarchy. The role of this draw path is unresolved; it is not established as the normal world-scene geometry pass. This does not establish which runtime object an RMC/RMI map reference selects, how map-derived transforms combine with the parent-aware pose, how shaders interpret submitted data, or whether other backends behave equivalently.

Geometry candidates are collected from an already constructed runtime scene hierarchy. The relationship between authored map references and those candidates remains unverified; this does not establish that the collection is exhaustive or represents the normal gameplay scene.

`bind_scene_instances_to_runtime` provides a narrow identity/resource receipt after complete runtime directory construction. Each receipt carries the scene-instance index and its map layer, entry, role, descriptor offset, and geometry reference alongside the map-selected owner and geometry-providing owner. Those owners can differ when a map handle selects a container whose descendants provide primitive geometry. The binding checks the decoded map-handle slot against the selected GMS row and its canonical runtime resource, then checks the geometry owner's resource, source pose, and authored parent links. If the asset carries authored parent provenance, that direct parent must also match the runtime GMS hierarchy. The caller must supply an asset built from the same GMS directory as the runtime and call before gameplay updates change source poses. This is a structural parser-to-runtime identity join; it does not establish that the original renderer selects the same owner, or establish map placement, world transforms, camera/view selection, visibility, draw order, or backend acceptance.

The source-only diagnostic GPU plan preserves both the map-selected source index and geometry-provider source index when copying scene instances. This keeps identity provenance available through that diagnostic conversion; the GPU plan still does not establish world transforms or normal-scene draw admission.

## Active view and draw boundary

The named `MainCamera` source is registered through the active-view queue with the exact object pointer obtained from the database lookup. The queue preserves that pointer, then drains it into a renderer-specific camera-view entry. For a Direct3D view, the entry is concretely a `ZCameraViewEntryD3D`; its ordinary callback traverses geometry-view entries and registers candidate records against the active view. This establishes the named camera's participation in active-view setup, not that it is the provider receiver for every generic active-view row.

The provider path is branch-dependent. When the camera provider returns null, the view uses the registered database root; the GMS loader and view read that same registered object. GMS-attached children can therefore be traversed into candidate registration on this branch, subject to component, flag, and culling checks. This proves a conditional GMS-owner-to-candidate path, but not that every authored row qualifies or that an RMC/RMI row selects a particular owner. On a non-null provider return, the exact provider object's class and its identity with the named camera remain unproven. The shared queue/wrapper path does not justify treating all entries as camera objects or assuming that provider output and GMS geometry identity are interchangeable.

The reviewed normal candidate path reaches resource and callback/state processing, and view maintenance can prepare renderer callback instances for candidates. The source-row data has not been identity-joined to those candidate descriptors or to an instance selected for ordinary visible drawing. The static trace has not joined normal records to the renderer's accepted-entry drain and a visible device draw. A separate shadow submission route does reach a concrete backend draw, but it is not evidence for the normal world geometry pass. Therefore camera participation and candidate preparation are partially established; camera-to-normal-draw parity is not.

A later bounded review closes the ordinary frame into a per-view command
consumer that dispatches callbacks attached to view-local commands. That command
route remains distinct from the inspected scene-geometry records: its callback
has not been identity-joined to a map-selected geometry resource or a visible
device draw. Reaching this callback consumer therefore does not establish
normal-scene parity or provide temporal-upscaler inputs. The known shadow
submission remains a separate route.

## Work required

- Join RMC/RMI map entries to the live scene-owner identities and established parent ancestry; the conditional GMS hierarchy path alone does not establish map-row selection.
- Join the non-null provider receiver to an exact runtime class and source identity; keep the null-provider root traversal branch-specific.
- Join authored row data to the active-view candidate descriptor and resulting renderer instance.
- Join normal candidate registration to the accepted-entry consumer and a visible backend draw, with the same view/camera identity.
- Produce world-space transforms and ordered draw records from the same admitted scene state.
- Keep diagnostic rendering separate from normal scene admission and temporal-upscaler inputs.

This document intentionally omits executable analysis, binary layouts, offsets, retail values, and corpus measurements. See [CLEAN_ROOM.md](../CLEAN_ROOM.md) for publication rules and [MODERN_GRAPHICS.md](MODERN_GRAPHICS.md) for the current renderer boundary.
