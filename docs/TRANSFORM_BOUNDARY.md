# Scene transform boundary

The normal world-scene producer is not yet connected to an admitted runtime scene. The diagnostic renderer must remain labelled as diagnostic; it is not a gameplay transform or a parity reference.

Map records, runtime object identity, parent relationships, camera state, accepted draw records, and backend submission must be joined by explicit project-owned interfaces before a normal world frame can be produced. The diagnostic renderer composes authored GMS source poses through their parent chain, while keeping source and map transforms separate; that composition is not a complete gameplay/world transform and does not establish parity.

## Authored scene ownership

Scene object hierarchy follows authored containment. Objects nested beneath a group are attached to that group; nested groups own their descendants. Returning to an enclosing level resumes that enclosing group as the parent.

This establishes parent/child ownership only. It does not establish which runtime object an RMC/RMI map reference selects, how map-derived transforms combine with the parent-aware pose, or which camera/view state applies to a given draw.

On one reviewed Direct3D draw path, submitted item positions derive from an object pose that incorporates its authored parent hierarchy. The role of this draw path is unresolved; it is not established as the normal world-scene geometry pass. This does not establish which runtime object an RMC/RMI map reference selects, how map-derived transforms combine with the parent-aware pose, how shaders interpret submitted data, or whether other backends behave equivalently.

Geometry candidates are collected from an already constructed runtime scene hierarchy. The relationship between authored map references and those candidates remains unverified; this does not establish that the collection is exhaustive or represents the normal gameplay scene.

`bind_scene_instances_to_runtime` provides a narrow identity/resource receipt after complete runtime directory construction. Each receipt carries the scene-instance index and its map layer, entry, role, descriptor offset, and geometry reference alongside the map-selected owner and geometry-providing owner. Those owners can differ when a map handle selects a container whose descendants provide primitive geometry. The binding checks the decoded map-handle slot against the selected GMS row and its canonical runtime resource, then checks the geometry owner's resource, source pose, and exact authored parent identity. A hierarchy-free asset can bind a top-level source row, but child rows fail closed because their parent provenance is absent. The caller must supply an asset built from the same GMS directory as the runtime and call before gameplay updates change source poses. This is a structural parser-to-runtime identity join; it does not establish that the original renderer selects the same owner, or establish map placement, world transforms, camera/view selection, visibility, draw order, or backend acceptance.

The diagnostic GPU plan preserves both the map-selected source index and geometry-provider source index when copying scene instances, and uses the composed GMS source pose for diagnostic placement. Map-entry placement and camera/view transforms remain separate. This keeps identity provenance available through that diagnostic conversion; the GPU plan still does not establish complete world transforms or normal-scene draw admission.

## Active view and draw boundary

The named `MainCamera` source is registered through the active-view queue with the exact object pointer obtained from the database lookup. The queue preserves that pointer, then drains it into a renderer-specific camera-view entry. For a Direct3D view, the entry is concretely a `ZCameraViewEntryD3D`; its ordinary callback traverses geometry-view entries and registers candidate records against the active view. This establishes the named camera's participation in active-view setup, not that it is the provider receiver for every generic active-view row.

The provider path is branch-dependent. When the camera provider returns null, the view uses the registered database root; the GMS loader and view read that same registered object. GMS-attached children can therefore be traversed into candidate registration on this branch, subject to component, flag, and culling checks. This proves a conditional GMS-owner-to-candidate path, but not that every authored row qualifies or that an RMC/RMI row selects a particular owner. On a non-null provider return, the exact provider object's class and its identity with the named camera remain unproven. The shared queue/wrapper path does not justify treating all entries as camera objects or assuming that provider output and GMS geometry identity are interchangeable.

The renderer has class-specific callback factories and device-submit methods on some branches, but their existence does not prove that an ordinary scene candidate receives one of those callback instances. The source-row data has not been identity-joined to the candidate descriptor and callback kind selected for visible drawing. On the identity-followed provider-null fallback path, the normal view command consumer reaches the same pooled child record but finds no callback instance and skips that callback. A separate shadow submission route does reach a concrete backend draw, but it is not evidence for the normal world geometry pass. Camera participation and candidate preparation are therefore partial; normal visible geometry submission remains unproven.

A later bounded review closes the ordinary frame into a per-view command
consumer and preserves the identity of its candidate child through command
ordering. On that reviewed producer path, the child has no non-null geometry
callback when consumed, so the nested geometry callback and visible device draw
are not reached. No later writer has been joined to this exact child before
consumption; other receiver paths and modules remain outside this result. This
does not establish normal-scene parity or provide temporal-upscaler inputs. The
known shadow submission remains a separate route.

## Work required

- Join RMC/RMI map entries to the live scene-owner identities and established parent ancestry; the conditional GMS hierarchy path alone does not establish map-row selection.
- Join the non-null provider receiver to an exact runtime class and source identity; keep the null-provider root traversal branch-specific.
- Join authored row data to the active-view candidate descriptor and resulting renderer instance.
- Join normal candidate registration to the accepted-entry consumer and a visible backend draw, with the same view/camera identity.
- Produce world-space transforms and ordered draw records from the same admitted scene state.
- Keep diagnostic rendering separate from normal scene admission and temporal-upscaler inputs.

This document intentionally omits executable analysis, binary layouts, offsets, retail values, and corpus measurements. See [CLEAN_ROOM.md](../CLEAN_ROOM.md) for publication rules and [MODERN_GRAPHICS.md](MODERN_GRAPHICS.md) for the current renderer boundary.
