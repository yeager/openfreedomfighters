# Scene render asset

`SceneRenderAsset` is the first owning, renderer-facing representation of all
directly materializable geometry in a scene. It bridges the parsed `RMC`/`RMI`,
`GMS`, `PRM`, and `TEX` models without retaining spans or pointers into parser
storage.

## Construction contract

`build_scene_render_asset` accepts the paired scene primitive and texture
catalogs, the scene GMS object-source directory, and an ordered list of RMC/RMI
map views. It:

- visits map layers in caller order, map entries in source order, and each
  required primary reference before its present optional secondary reference;
- retains a resolution record for every handle, including unresolved and
  non-primitive outcomes;
- creates an instance for a direct local ordinary PRM, or for an ordinary PRM
  below a map-resolved `ZGROUP` or `ZROOM` container in the authored GMS
  hierarchy;
- deduplicates mesh storage by PRM catalog entry and texture storage by TEX
  catalog entry while never deduplicating instances;
- preserves RMC/RMI kind, layer, descriptor, role, handle, GMS source identity,
  topology, alpha evidence, and map/GMS transforms; and
- keeps the GMS and map transforms separate because their composition has not
  yet been established from evidence.

Executable tracing confirms that the original loader copies the GMS basis into
runtime matrix storage, copies the GMS position, and passes both to the newly
allocated runtime object's transform setter before registration. It does not
yet establish the matrix
layout consumed by rendering, hierarchy or attachment propagation, world-space
composition, or final draw-matrix provenance. The asset therefore continues to
retain source and map transforms separately.

Container expansion is preorder in retained authored child order. It keeps the
root map reference and descendant source identity, but uses only the
descendant's existing GMS diagnostic transform. It does not compose container,
map, or child transforms.

The asset includes line lists, untextured meshes, and transparent geometry. The
diagnostic single-mesh preview's selection rules do not apply to scene assets.
Each source TEX mip is decoded once to an owned RGBA8 mip chain per referenced
TEX entry. Mesh vertices, indexes, and draw ranges are copied into owned
storage. The chain is retained and uploaded only by the source-only diagnostic
scene path; it does not establish retail material selection, LOD choice, or
faithful original rendering.

`load_scene_render_asset` applies this contract to any supplied scene archive.
It requires the paired PRM, TEX, and GMS resources plus both RMC and RMI layers,
preserves RMC before RMI in the map-layer order, and returns an asset with no
references into archive or parser storage. `load_startup_scene_render_asset`
retains the explicit startup-scene convenience path. Because that path is part
of normal startup ownership, it accepts only a regular, non-symlink archive at
the fixed checked location; a replacement link is rejected without exposing a
local path. The generic loader remains intentionally unrestricted for callers
that have already established their own archive ownership boundary.

Before any renderer-facing parser runs, the generic loader also admits only the
observed campaign package family: exactly one each of `ZGF`, `SUP`, `GMS`,
`TEX`, `PRM`, `RMC`, `RMI`, `SND`, `OCT`, and `SGP`; at most one each of `BUF`,
`LOC`, and `ANM`; and no other member family. It parses the required `ZGF` and
`SUP` inputs even though their runtime semantics are not yet part of the
diagnostic renderer. A nonempty GMS image requires exactly one BUF resource,
which is validated before an asset is returned; an empty image must not supply
one. This is source ownership and integrity checking, not a recovered scene
manager, campaign route, camera, or gameplay activation.

The current diagnostic runtime uses
`load_diagnostic_scene_render_asset`. It enumerates non-symlink ZIP files
below the installation's scene directory, applies a case-independent relative
path sort with an exact-path tie break, ignores archives that do not contain a
complete scene resource set, validates complete candidates, and selects the
first asset with at least one directly materialized instance. Invalid complete
scene archives remain fatal rather than being silently skipped. The selected
retail archive name is not logged or incorporated into public output.

## Diagnostic archive selection

Automatically choosing an archive with at least one directly materializable
instance is permitted only as a renderer-development policy. It does not recover
the retail startup sequence, level order, scene hierarchy, or camera. A public
selector should therefore use a neutral name such as
`load_diagnostic_scene_render_asset`, not `load_first_level` or
`load_gameplay_scene`.

Selection must be deterministic over the verified installation and based only on
validated structural results: required scene-resource families parse, the owning
scene asset validates, and its instance and draw-command sets are non-empty.
Malformed candidates are errors rather than reasons to skip silently. A valid but
empty candidate may be skipped, and failure to find an eligible candidate must be
reported without falling back to unrelated geometry. The chosen archive path may
remain transient local provenance, but it must not become a stable gameplay ID.

Public logs should say `selected a structurally eligible source-only diagnostic
scene` and may include aggregate mesh, instance, texture, and draw counts. They
must not print a retail archive name, path, member name, object identifier, or a
claim that the scene is the first, startup, campaign, or representative level.
Tests should use project-authored archives whose ordering, empty candidates,
malformed candidates, and selected counts are controlled explicitly.

Aggregate selection measurements are safe to publish under the repository's
clean-room policy when they reveal only counts or boolean coverage across the
supported corpus. The identity, ordinal, filename, path, hashes, and extracted
content of the selected retail archive are unnecessary for interoperability and
should remain out of public selection reports.

## Validation and limits

Construction reuses `RenderAssetBindings` for cross-catalog identity and topology
validation, then rejects invalid vertex indexes and non-finite vertex attributes,
source transforms, map transforms, or extents. Checked cumulative budgets cap a
scene at 16 map layers, 131,072 resolution records, 16,000,000 deduplicated
vertices, 32,000,000 deduplicated indexes, 4,000,000 deduplicated draw ranges,
16 source mip levels per texture, and 1 GiB of decoded RGBA data across every
retained mip. These are defensive implementation
limits, not claims about the file format.

`validate_scene_render_asset` independently repeats the GPU-boundary invariants
for an already owned asset. It checks all mesh, texture, resolution, and instance
references; exact ordered, halving RGBA8 mip-chain storage; finite attributes and transforms; topology-aware
contiguous draw ranges; vertex indexes; provenance links; and the same cumulative
budgets. Upload code must call this validator even when the asset originally came
from the builder, because future cache and tooling paths may construct or mutate
the public aggregate directly.

A read-only audit of all 90 supported scenes observed 220 directly
materializable instances in total and a per-scene maximum of 106. Independent
per-scene maxima were 97 deduplicated PRMs, two deduplicated textures, 1,923
vertices, 2,714 indexes, 97 primitive draw ranges, 106 instance-expanded draws,
and 262,144 decoded RGBA bytes. These figures exclude external, unresolved, and
local non-primitive references; independent maxima need not occur in one scene.

## Current boundary

This model prepares the complete set of currently understood direct local scene
primitives, and the SDL GPU runtime uploads and submits every retained
source-only diagnostic draw. Applying evidenced transform composition,
reconstructing materials, selecting the scene camera, and resolving indirect or
non-primitive GMS sources are later milestones. Modern and Modern+ render paths
must consume the same instance identity and gameplay snapshot as Original mode.
The current source-only diagnostic transform is a presentation convention, not a
recovered world-space formula. Aggregate overlap experiments are useful for
rejecting candidates, but cannot override the executable's query arithmetic or
establish map placement: even a speculative additive root bias substantially
improves one candidate while contradicting the recovered query path. The exact
proven relationships, prohibited interpretations, and evidence gates are
documented in [TRANSFORM_BOUNDARY.md](TRANSFORM_BOUNDARY.md).

## SDL-free GPU plan

`prepare_scene_gpu_plan` converts a validated asset into a deterministic command
schedule without depending on SDL or a graphics device. It computes one global
fit over every indexed vertex of every materialized instance, owns normalized GPU
vertex data and one copy of each deduplicated mesh/texture resource, and emits one
command per preserved draw range. Opaque
commands remain in stable instance order and use depth test/write; non-opaque
commands follow in stable order with blending and depth test but no depth writes.
Untextured commands retain an empty texture index so the backend can bind one
explicit shared white fallback. Every GPU instance also retains its expected
mesh index, and independent validation rejects draws that pair an otherwise
valid instance with the wrong mesh resource.

The diagnostic projection preserves a real third coordinate in SDL GPU's
`[0, 1]` clip-depth convention rather than flattening every object to one plane.
`make_scene_diagnostic_matrices` exposes the same calculation as two row-major
matrices in the bundled shader's `position * model * projection_view` order, so
backends do not need to reconstruct or transpose the source-only convention.
It reserves `[0.05, 0.95]` for geometry and handles planar depth with a stable
midpoint. Physical viewport dimensions produce an aspect-correct uniform without
rebuilding geometry resources, and zero-sized viewports are rejected. The fit
uses only the explicitly named source-only diagnostic transform;
RMC/RMI transform values cannot influence it. This CPU-only boundary makes global
bounds, instance ordering, topology, alpha/depth policy, and eventual resize-aware
uniform generation testable identically on Linux, macOS, and Windows.

## SDL runtime integration contract

The SDL GPU runtime consumes the validated plan as a multi-instance
**source-only diagnostic scene**. The runtime may upload each deduplicated mesh
and every validated source mip of each texture once, bind the shared white fallback for untextured commands, and
submit every planned command in its established diagnostic schedule. It must use
the plan's global fit and per-instance source diagnostic positions unchanged;
RMC/RMI transforms must remain retained evidence and must not influence GPU
placement.

Runtime output must call this view a `source-only diagnostic scene`. Success
messages may report materialized instance, mesh, texture, and draw-command counts,
but must not call the result a loaded level, recovered scene, Original rendering,
or faithful world placement. Missing, external, and non-primitive sources remain
reported resolution outcomes rather than placeholder draws. Only the two
evidence-backed container types may expose direct-primitive descendants; other
non-primitive sources are not expanded. A zero-instance plan is a valid
diagnostic result and must not silently fall back to the older single-mesh
preview.

This integration does not establish camera matrices, map/GMS transform
composition, material semantics, lighting, transparent ordering fidelity, or
line-list presentation fidelity. Those remain independently gated by the evidence
requirements in [TRANSFORM_BOUNDARY.md](TRANSFORM_BOUNDARY.md).
